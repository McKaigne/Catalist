#include <format>
#include <ostream>

#include "cli/common.hpp"
#include "cli/refs.hpp"
#include "core/text.hpp"
#include "web/anilist.hpp"

namespace catalist::cli {

namespace {

// The list `add` and `search` work in: anime unless -m says manga. Nothing
// when both -a and -m are given.
std::optional<MediaType> OneType(const args::Parsed& line) {
	if (line.Has("a") and line.Has("m"))
		return std::nullopt;
	return line.Has("m") ? MediaType::kManga : MediaType::kAnime;
}

// The title as AniList has it: its name, both ids and the totals.
Entry FromMatch(Entry blank, const anilist::Match& match) {
	blank.title = match.name;
	blank.anilist_id = match.anilist_id;
	blank.mal_id = match.mal_id;
	blank.total_units = match.total_units;
	blank.total_volumes = match.total_volumes;
	return blank;
}

// "28 episodes", or "? chapters" while the total is not known.
std::string LengthOf(const anilist::Match& match, MediaType type) {
	return std::format("{} {}", match.total_units > 0 ? std::to_string(match.total_units) : "?",
	                   UnitName(type, match.total_units));
}

// The match `add` is to take: the only one, the one `pick` numbers, or the
// one chosen at a terminal. Nothing, with the reason on standard error, when
// there is no such match.
std::optional<anilist::Match> ChooseMatch(CliContext& ctx, MediaType type, const std::string& query,
                                          std::optional<int> pick) {
	auto found = anilist::Search(ctx.net, type, query);
	if (not found.has_value()) {
		Fail(ctx, found.error());
		ctx.err << "To add a title without AniList: catalist add --no-lookup TITLE\n";
		return std::nullopt;
	}
	const auto& matches = found.value();
	if (matches.empty()) {
		Fail(ctx, std::format("AniList has no {} matching '{}'", TypeKey(type), query));
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

	ctx.err << std::format("{}'{}' matches {} {} on AniList:\n",
	                       ctx.interactive ? "" : "error: ", query, matches.size(), TypeKey(type));
	for (size_t i = 0; i < matches.size(); ++i)
		ctx.err << std::format("  {:>2}  {:<4}  {}  ({})\n", i + 1,
		                       matches[i].year.empty() ? "-" : matches[i].year, matches[i].name,
		                       LengthOf(matches[i], type));
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

// A title added by hand has no id to know it by: its name is all there is
// to warn about.
void NoteSameName(CliContext& ctx, const Entry& added) {
	std::string name = utf8::Lowercase(added.title);
	for (const auto& e : ctx.db.GetEntries(added.type)) {
		if (utf8::Lowercase(e.title) != name)
			continue;
		ctx.err << std::format("note: your {} list has a title of that name already, {}.\n",
		                       TypeKey(added.type), refs::Of(e));
		return;
	}
}

int RunAdd(const Command& self, const args::Parsed& line, CliContext& ctx) {
	auto type = OneType(line);
	if (not type)
		return UsageError(ctx, self, "give -a or -m, not both");
	auto name = ReadText(self, line, ctx);
	if (not name)
		return 2;
	if (name->empty())
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
	bool by_hand = line.Has("no-lookup");
	if (by_hand and pick)
		return UsageError(ctx, self,
		                  "--pick chooses among AniList's matches; --no-lookup asks none");

	IsoDate today = ctx.Today();
	Entry added;
	added.type = *type;
	added.title = *name;
	added.status = WatchStatus::kPlanToWatch;
	added.updated_at = today;
	// AniList is asked before the catalogue is taken for writing: no other
	// program waits while the person chooses.
	if (not by_hand) {
		auto match = ChooseMatch(ctx, *type, added.title, pick);
		if (not match)
			return 1;
		added = FromMatch(added, *match);
	}
	// In the order `set` takes them: the progress starts the title, or
	// completes it at the total, and a status that is given stands.
	if (progress)
		added = SetCount(added, CountField::kProgress, *progress, today);
	if (line.Has("status") or not progress)
		added = SetStatus(added, *status, today);
	if (score)
		added = SetScore(added, *score, today);

	if (not LoadForWrite(ctx))
		return 1;
	if (auto mine = ctx.db.FindByIds(*type, added.anilist_id, added.mal_id)) {
		ctx.err << std::format("{} is in your {} list already, as {} ({}).\nNothing changed.\n",
		                       DisplayName(*mine), TypeKey(*type), refs::Of(*mine),
		                       StatusKey(mine->status));
		return 0;
	}
	if (by_hand)
		NoteSameName(ctx, added);
	added.id = ctx.db.AddEntry(added);
	return SaveAndReport(ctx, line, {{added.type, added.id, std::nullopt, added}});
}

// The matches as a table for a terminal.
std::string MatchTable(const CliContext& ctx, MediaType type,
                       const std::vector<anilist::Match>& matches,
                       const std::vector<std::string>& mine) {
	std::vector<output::TableColumn> columns = {
	    {"#", true},
	    {"YEAR"},
	    {"TITLE", false, true},
	    {type == MediaType::kAnime ? "EPISODES" : "CHAPTERS", true},
	    {"SCORE", true},
	    {"YOURS"}};
	std::vector<std::vector<output::Styled>> rows;
	for (size_t i = 0; i < matches.size(); ++i) {
		const auto& match = matches[i];
		rows.push_back({{std::to_string(i + 1), output::kCyan},
		                {match.year.empty() ? "-" : match.year},
		                {match.name},
		                {match.total_units > 0 ? std::to_string(match.total_units) : "?"},
		                {output::Stars(match.score), output::kYellow},
		                {mine[i], output::kGreen}});
	}
	return output::Table(columns, rows, ctx.columns, ctx.color);
}

int RunSearch(const Command& self, const args::Parsed& line, CliContext& ctx) {
	auto type = OneType(line);
	if (not type)
		return UsageError(ctx, self, "give -a or -m, not both");
	auto query = ReadText(self, line, ctx);
	if (not query)
		return 2;
	if (query->empty())
		return UsageError(ctx, self, "give the QUERY to search for");
	if (not Load(ctx))
		return 1;
	auto found = anilist::Search(ctx.net, *type, *query);
	if (not found.has_value())
		return Fail(ctx, found.error());
	const auto& matches = found.value();
	std::vector<std::string> mine;
	for (const auto& match : matches) {
		auto tracked = ctx.db.FindByIds(*type, match.anilist_id, match.mal_id);
		mine.push_back(tracked ? refs::Of(*tracked) : "");
	}

	if (ctx.columns > 0) {
		if (matches.empty()) {
			ctx.err << std::format("AniList has no {} matching '{}'.\n", TypeKey(*type), *query);
			return 0;
		}
		ctx.out << MatchTable(ctx, *type, matches, mine);
		ctx.err << std::format("Add one with: catalist add{} --pick=N QUERY\n",
		                       *type == MediaType::kManga ? " -m" : "");
		return 0;
	}
	std::vector<std::vector<std::string>> rows;
	for (size_t i = 0; i < matches.size(); ++i) {
		const auto& match = matches[i];
		rows.push_back({std::to_string(i + 1), TypeKey(*type), match.name,
		                output::OrDash(match.year), std::to_string(match.total_units),
		                match.score > 0.0 ? std::format("{:.1f}", match.score) : "0",
		                std::to_string(match.anilist_id), std::to_string(match.mal_id),
		                output::OrDash(mine[i])});
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
	         std::string("Searches AniList for TITLE, an anime unless -m is given, and adds the\n"
	                     "match: the only one, the one you choose when there are several, or the\n"
	                     "one --pick names by its number in that list ('search' prints the same\n"
	                     "list). Where nobody can be asked, several matches without --pick are an\n"
	                     "error. A new title is planned, or watching once it has a --progress,\n"
	                     "unless --status says otherwise. A title that is in your list already\n"
	                     "is left as it is.\n\n"
	                     "With --no-lookup AniList is not asked: TITLE is added as it is written,\n"
	                     "without ids or totals ('set' can give it a total).\n\n") +
	         kChangeFormat,
	     .options = {{'a', "", "", "add an anime (the default)"},
	                 {'m', "", "", "add a manga"},
	                 {0, "pick", "N", "take the Nth match without asking"},
	                 {0, "status", "STATUS",
	                  "watching, completed, on-hold, dropped, planned or rewatching"},
	                 {0, "score", "N", "give it a score as well, 0 to 10"},
	                 {0, "progress", "N", "the episodes or chapters done already"},
	                 {0, "no-lookup", "", "add TITLE as written, without asking a site"},
	                 kQuiet},
	     .operands = "TITLE...",
	     .run = RunAdd},
	    {.name = "search",
	     .summary = "Search AniList without adding anything",
	     .details = "Prints up to ten matches for QUERY on AniList, anime unless -m is given,\n"
	                "best first. At a terminal they are drawn as a table. Into a pipe each is\n"
	                "one line of fields with a tab between them:\n"
	                "  pick type title year total score anilist_id mal_id ref\n"
	                "pick is the N of 'add --pick=N'; ref is the title in your lists, or '-'.\n"
	                "A year that is not known is '-', a total, a score or an id that is not\n"
	                "is 0.\n",
	     .options = {{'a', "", "", "search anime (the default)"}, {'m', "", "", "search manga"}},
	     .operands = "QUERY...",
	     .run = RunSearch},
	};
}

}  // namespace catalist::cli
