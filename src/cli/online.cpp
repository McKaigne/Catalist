#include <format>
#include <ostream>

#include "cli/common.hpp"
#include "cli/refs.hpp"
#include "core/text.hpp"
#include "web/anilist.hpp"

namespace catalist::cli {

namespace {

const std::string& ShownName(const anilist::Match& match) {
	return match.english.empty() ? match.name : match.english;
}

Entry FromMatch(Entry blank, const anilist::Match& match) {
	blank.type = match.type;
	blank.title = match.name;
	blank.english_title = match.english;
	blank.anilist_id = match.anilist_id;
	blank.mal_id = match.mal_id;
	blank.total_units = match.total_units;
	blank.total_volumes = match.total_volumes;
	return blank;
}

std::string LengthOf(const anilist::Match& match) {
	return std::format("{} {}", match.total_units > 0 ? std::to_string(match.total_units) : "?",
	                   UnitName(match.type, match.total_units));
}

std::string KindOf(std::optional<MediaType> only) {
	return only ? TypeKey(*only) : "title";
}

std::string HeadingOf(MediaType type) {
	return type == MediaType::kAnime ? "ANIME" : "MANGA";
}

std::optional<anilist::Match> OnlyExact(const std::vector<anilist::Match>& matches,
                                        const std::string& query) {
	std::string wanted = utf8::Folded(query);
	std::vector<anilist::Match> exact;
	for (const auto& match : matches)
		if (utf8::Folded(match.name) == wanted or utf8::Folded(match.english) == wanted)
			exact.push_back(match);
	return exact.size() == 1 ? std::optional(exact[0]) : std::nullopt;
}

void PrintMatches(CliContext& ctx, const std::vector<anilist::Match>& matches,
                  std::optional<MediaType> only) {
	std::optional<MediaType> section;
	for (size_t i = 0; i < matches.size(); ++i) {
		if (not only and section != matches[i].type) {
			section = matches[i].type;
			ctx.err << HeadingOf(*section) << "\n";
		}
		ctx.err << std::format("  {:>2}  {:<4}  {}  ({})\n", i + 1,
		                       matches[i].year.empty() ? "-" : matches[i].year,
		                       ShownName(matches[i]), LengthOf(matches[i]));
	}
}

std::optional<anilist::Match> ChooseMatch(CliContext& ctx, std::optional<MediaType> only,
                                          const std::string& query, std::optional<int> pick) {
	auto found = anilist::Search(ctx.net, only, query);
	if (not found.has_value()) {
		Fail(ctx, found.error());
		return std::nullopt;
	}
	const auto& matches = found.value();
	if (matches.empty()) {
		Fail(ctx, std::format("AniList has no {} matching '{}'", KindOf(only), query));
		return std::nullopt;
	}
	if (pick and static_cast<size_t>(*pick) > matches.size()) {
		Fail(ctx, std::format(
		              "--pick={}, but AniList has only {} for '{}'", *pick,
		              matches.size() == 1 ? "one match" : std::format("{} matches", matches.size()),
		              query));
		return std::nullopt;
	}
	if (pick)
		return matches[static_cast<size_t>(*pick) - 1];
	if (matches.size() == 1)
		return matches[0];
	if (ctx.interactive) {
		if (auto exact = OnlyExact(matches, query))
			return exact;
	}

	ctx.err << std::format("{}'{}' matches {} {} on AniList:\n", ctx.interactive ? "" : "error: ",
	                       query, matches.size(), only ? TypeKey(*only) : "titles");
	PrintMatches(ctx, matches, only);
	if (not ctx.interactive) {
		ctx.err << "Take one by its number with --pick=N.\n";
		return std::nullopt;
	}
	auto chosen = AskWhich(ctx, matches.size());
	if (not chosen) {
		ctx.err << "Cancelled.\n";
		return std::nullopt;
	}
	return matches[*chosen];
}

int RunAdd(const Command& self, const args::Parsed& line, CliContext& ctx) {
	if (line.Has("a") and line.Has("m"))
		return UsageError(ctx, self, "give -a or -m, not both");
	auto names = NamesIn(line.operands);
	if (names.empty())
		return UsageError(ctx, self, "name the title to add");
	auto status = StatusNamed(line.Has("status") ? line.Get("status") : "planned");
	if (not status)
		return UsageError(ctx, self, std::format("unknown status '{}'", line.Get("status")));
	std::optional<int> pick;
	if (line.Has("pick")) {
		pick = args::ParseCount(line.Get("pick"));
		if (not pick or *pick == 0)
			return UsageError(
			    ctx, self,
			    std::format("--pick '{}' is not the number of a match", line.Get("pick")));
		if (names.size() > 1)
			return UsageError(ctx, self, "--pick takes one title at a time");
	}
	std::optional<int> score;
	if (line.Has("score")) {
		score = args::ParseCount(line.Get("score"));
		if (not score or *score > 10)
			return UsageError(ctx, self,
			                  std::format("--score '{}' is not 0 to 10", line.Get("score")));
	}
	std::optional<int> progress;
	if (line.Has("progress")) {
		progress = args::ParseCount(line.Get("progress"));
		if (not progress)
			return UsageError(ctx, self,
			                  std::format("--progress '{}' is not a count", line.Get("progress")));
	}

	IsoDate today = ctx.Today();
	std::vector<Entry> wanted;
	// AniList is asked before the catalogue is taken for writing: no other
	// program waits while the person chooses.
	for (const auto& name : names) {
		auto match = ChooseMatch(ctx, OnlyType(line), name, pick);
		if (not match)
			return 1;
		Entry added = FromMatch({}, *match);
		added.status = WatchStatus::kPlanToWatch;
		added.updated_at = today;
		// In the order `set` takes them: the progress starts the title, or
		// completes it at the total, and a status that is given stands.
		if (progress)
			added = SetCount(added, CountField::kProgress, *progress, today);
		if (line.Has("status") or not progress)
			added = SetStatus(added, *status, today);
		if (score)
			added = SetScore(added, *score, today);
		wanted.push_back(added);
	}

	if (not LoadForWrite(ctx))
		return 1;
	std::vector<journal::Change> changes;
	for (auto& added : wanted) {
		if (auto mine = ctx.db.FindByIds(added.type, added.anilist_id, added.mal_id)) {
			if (not line.Has("q"))
				ctx.err << std::format("{} is in your {} list already ({}).\n", DisplayName(*mine),
				                       TypeKey(added.type), StatusKey(mine->status));
			continue;
		}
		added.id = ctx.db.AddEntry(added);
		changes.push_back({added.type, added.id, std::nullopt, added});
	}
	return SaveAndReport(ctx, line, changes);
}

std::string MatchTable(const CliContext& ctx, const std::vector<anilist::Match>& matches,
                       const std::vector<std::string>& mine) {
	std::vector<output::TableColumn> columns = {
	    {"", true}, {""}, {"", false, true}, {"", true}, {""}, {""}};
	std::vector<output::Section> sections = {{HeadingOf(MediaType::kAnime), {}},
	                                         {HeadingOf(MediaType::kManga), {}}};
	for (size_t i = 0; i < matches.size(); ++i) {
		const auto& match = matches[i];
		sections[match.type == MediaType::kAnime ? 0 : 1].rows.push_back(
		    {{std::to_string(i + 1), output::kCyan},
		     {match.year.empty() ? "-" : match.year},
		     {ShownName(match)},
		     {LengthOf(match)},
		     {output::Stars(match.score), output::kYellow},
		     {mine[i], output::kGreen}});
	}
	return output::Sections(columns, sections, ctx.columns, ctx.color);
}

int RunSearch(const Command& self, const args::Parsed& line, CliContext& ctx) {
	if (line.Has("a") and line.Has("m"))
		return UsageError(ctx, self, "give -a or -m, not both");
	auto only = OnlyType(line);
	std::string query = ReadText(line);
	if (query.empty())
		return UsageError(ctx, self, "give the QUERY to search for");
	if (not Load(ctx))
		return 1;
	auto found = anilist::Search(ctx.net, only, query);
	if (not found.has_value())
		return Fail(ctx, found.error());
	const auto& matches = found.value();
	std::vector<std::string> mine;
	for (const auto& match : matches) {
		auto tracked = ctx.db.FindByIds(match.type, match.anilist_id, match.mal_id);
		std::string yours = ctx.columns > 0 ? "" : "-";
		if (tracked)
			yours =
			    ctx.columns > 0 ? StatusToString(tracked->status, match.type) : refs::Of(*tracked);
		mine.push_back(yours);
	}

	if (ctx.columns > 0) {
		if (matches.empty()) {
			ctx.err << std::format("AniList has no {} matching '{}'.\n", KindOf(only), query);
			return 0;
		}
		ctx.out << MatchTable(ctx, matches, mine);
		ctx.err << std::format("Add one with: catalist add{} --pick=N QUERY\n",
		                       only ? (*only == MediaType::kManga ? " -m" : " -a") : "");
		return 0;
	}
	std::vector<std::vector<std::string>> rows;
	for (size_t i = 0; i < matches.size(); ++i) {
		const auto& match = matches[i];
		rows.push_back({std::to_string(i + 1), TypeKey(match.type), ShownName(match),
		                output::OrDash(match.year), std::to_string(match.total_units),
		                match.score > 0.0 ? std::format("{:.1f}", match.score) : "0",
		                std::to_string(match.anilist_id), std::to_string(match.mal_id), mine[i]});
	}
	WriteRows(ctx, rows);
	return 0;
}

}  // namespace

std::vector<Command> OnlineCommands() {
	return {
	    {.name = "add",
	     .summary = "Add a title found on AniList",
	     .details =
	         std::string("Searches AniList for each TITLE, anime and manga unless -a or -m is\n"
	                     "given, and adds the match: the only one, at a terminal the one with\n"
	                     "exactly that name, the one you choose, or the one --pick names by its\n"
	                     "number ('search' prints the same list). Commas separate titles, and\n"
	                     "when one fails nothing is added. Where nobody can be asked, several\n"
	                     "matches without --pick are an error. A new title is planned, or\n"
	                     "watching once it has a --progress, unless --status says otherwise. A\n"
	                     "title that is in your list already is left as it is.\n\n") +
	         kChangeFormat,
	     .options = {{'a', "", "", "look for anime only"},
	                 {'m', "", "", "look for manga only"},
	                 {0, "pick", "N", "take the Nth match without asking"},
	                 {0, "status", "STATUS",
	                  "watching, completed, on-hold, dropped, planned or rewatching"},
	                 {0, "score", "N", "give it a score as well, 0 to 10"},
	                 {0, "progress", "N", "the episodes or chapters done already"},
	                 kQuiet},
	     .operands = "TITLE...",
	     .run = RunAdd},
	    {.name = "search",
	     .summary = "Search AniList without adding anything",
	     .details = "Prints up to five anime and five manga for QUERY, or ten with -a or -m,\n"
	                "best first and numbered straight through. Adult titles are left out. At\n"
	                "a terminal they are drawn as a table. Into a pipe each is one line of\n"
	                "fields with a tab between them:\n"
	                "  pick type title year total score anilist_id mal_id ref\n"
	                "pick is the N of 'add --pick=N'; ref is the title in your lists, or '-'.\n"
	                "A year that is not known is '-', a total, a score or an id that is not\n"
	                "is 0.\n",
	     .options = {{'a', "", "", "search anime only"}, {'m', "", "", "search manga only"}},
	     .operands = "QUERY...",
	     .run = RunSearch},
	};
}

}  // namespace catalist::cli
