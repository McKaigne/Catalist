#include <fmt/format.h>

#include <ostream>

#include "cli/common.hpp"
#include "cli/refs.hpp"
#include "core/text.hpp"

namespace catalist::cli {

namespace {

// What to say when a request to AniList failed.
std::string webError(const std::string& message) {
	if (message == "could not reach AniList")
		return "could not reach AniList; check the connection and try again";
	return message;
}

// The list `add` and `search` work in: anime unless -m says manga. Nothing
// when both -a and -m are given.
std::optional<mediaType> oneType(const args::parsed& line) {
	if (line.has("a") and line.has("m"))
		return std::nullopt;
	return line.has("m") ? mediaType::kManga : mediaType::kAnime;
}

// "28 episodes", or "? chapters" while the total is not known.
std::string lengthOf(const anilist::title& match, mediaType type) {
	return fmt::format("{} {}", match.totalUnits > 0 ? std::to_string(match.totalUnits) : "?",
	                   unitName(type, match.totalUnits));
}

// The match `add` is to take: the only one, the one --pick names, or the one
// chosen at a terminal. Nothing, with the reason on standard error, when
// there is no such match.
std::optional<anilist::title> chooseMatch(cliContext& ctx, mediaType type, const std::string& query,
                                          std::optional<int> pick) {
	auto found = anilist::search(ctx.post, type, query);
	if (not found.ok()) {
		fail(ctx, webError(found.errorMessage()));
		ctx.err << fmt::format("To add a title without AniList: {} add --no-lookup TITLE\n",
		                       ctx.program);
		return std::nullopt;
	}
	const auto& matches = found.value();
	if (matches.empty()) {
		fail(ctx, fmt::format("AniList has no {} matching '{}'", typeKey(type), query));
		return std::nullopt;
	}
	if (pick and static_cast<size_t>(*pick) > matches.size()) {
		fail(ctx, fmt::format(
		              "--pick={}, but AniList has only {} for '{}'", *pick,
		              matches.size() == 1 ? "one match" : fmt::format("{} matches", matches.size()),
		              query));
		return std::nullopt;
	}
	if (pick)
		return matches[static_cast<size_t>(*pick) - 1];
	if (matches.size() == 1)
		return matches[0];

	ctx.err << fmt::format("{}'{}' matches {} {} on AniList:\n",
	                       ctx.interactive ? "" : "error: ", query, matches.size(), typeKey(type));
	for (size_t i = 0; i < matches.size(); ++i)
		ctx.err << fmt::format("  {:>2}  {:<4}  {}  ({})\n", i + 1,
		                       matches[i].year.empty() ? "-" : matches[i].year, matches[i].name,
		                       lengthOf(matches[i], type));
	if (not ctx.interactive) {
		ctx.err << "Take one by its number with --pick=N.\n";
		return std::nullopt;
	}
	auto chosen = askWhich(ctx, matches.size());
	if (not chosen) {
		ctx.err << "Cancelled.\n";
		return std::nullopt;
	}
	return matches[*chosen];
}

// The title as AniList has it: its name, both ids, the totals and how much
// is out. Fails with what to tell the person.
result<entry> lookedUp(cliContext& ctx, entry blank, const anilist::title& match) {
	auto details = anilist::lookUp(ctx.post, {blank.type, match.anilistId, match.malId});
	if (not details.ok())
		return result<entry>::failure(webError(details.errorMessage()));
	blank.title = details.value().name.empty() ? match.name : details.value().name;
	return result<entry>::success(applyWebFacts(blank, details.value().facts, ctx.today()));
}

// A title added by hand has no id to know it by: its name is all there is
// to warn about.
void noteSameName(cliContext& ctx, const entry& added) {
	std::string name = utf8::lowercase(added.title);
	for (const auto& e : ctx.db.getEntries(added.type)) {
		if (utf8::lowercase(e.title) != name)
			continue;
		ctx.err << fmt::format("note: your {} list has a title of that name already, {}.\n",
		                       typeKey(added.type), refs::of(e));
		return;
	}
}

int runAdd(const command& self, const args::parsed& line, cliContext& ctx) {
	auto type = oneType(line);
	if (not type)
		return usageError(ctx, self, "give -a or -m, not both");
	auto name = readText(self, line, ctx);
	if (not name)
		return 2;
	if (name->empty())
		return usageError(ctx, self, "name the title to add");
	auto status = parseStatus(line.has("status") ? line.get("status") : "planned");
	if (not status)
		return usageError(ctx, self, fmt::format("unknown status '{}'", line.get("status")));
	std::optional<int> pick;
	if (line.has("pick")) {
		pick = args::parseCount(line.get("pick"));
		if (not pick or *pick == 0)
			return usageError(
			    ctx, self,
			    fmt::format("--pick '{}' is not the number of a match", line.get("pick")));
	}
	std::optional<int> score;
	if (line.has("score")) {
		score = args::parseCount(line.get("score"));
		if (not score or *score > 10)
			return usageError(ctx, self,
			                  fmt::format("--score '{}' is not 0 to 10", line.get("score")));
	}
	std::optional<int> progress;
	if (line.has("progress")) {
		progress = args::parseCount(line.get("progress"));
		if (not progress)
			return usageError(ctx, self,
			                  fmt::format("--progress '{}' is not a count", line.get("progress")));
	}
	bool byHand = line.has("no-lookup");
	if (byHand and pick)
		return usageError(ctx, self,
		                  "--pick chooses among AniList's matches; --no-lookup has none");

	isoDate today = ctx.today();
	entry added;
	added.type = *type;
	added.title = *name;
	added.status = watchStatus::kPlanToWatch;
	added.updatedAt = today;
	// AniList is asked before the catalogue is taken for writing: no other
	// program waits while the person chooses.
	if (not byHand) {
		auto match = chooseMatch(ctx, *type, added.title, pick);
		if (not match)
			return 1;
		auto found = lookedUp(ctx, added, *match);
		if (not found.ok())
			return fail(ctx, found.errorMessage());
		added = found.value();
	}
	// In the order `set` takes them: the progress starts the title, or
	// completes it at the total, and a status that is given stands.
	if (progress)
		added = setCount(added, countField::kProgress, *progress, today);
	if (line.has("status") or not progress)
		added = setStatus(added, *status, today);
	if (score)
		added = setScore(added, *score, today);

	if (not loadForWrite(ctx))
		return 1;
	if (auto mine = ctx.db.findByIds(*type, added.anilistId, added.malId)) {
		ctx.err << fmt::format("{} is in your {} list already, as {} ({}).\nNothing changed.\n",
		                       mine->title, typeKey(*type), refs::of(*mine),
		                       statusKey(mine->status));
		return 0;
	}
	if (byHand)
		noteSameName(ctx, added);
	added.id = ctx.db.addEntry(added);
	return saveAndReport(ctx, line, {{added.type, added.id, std::nullopt, added}});
}

// The columns of a match of `search`. `pick` is its place in the list, the
// N of `add --pick=N`; `ref` is the title in your lists, when it is there.
const std::vector<std::string> kSearchColumns = {"pick",  "type",       "title",  "year", "total",
                                                 "score", "anilist_id", "mal_id", "ref"};

// AniList's score out of 10, to one decimal; 0 while there is none.
output::cell scoreCell(double score) {
	if (score <= 0.0)
		return {"0", output::jsonForm::kNull};
	return {fmt::format("{:.1f}", score), output::jsonForm::kNumber};
}

// The matches as a table for a terminal.
std::string matchTable(const cliContext& ctx, mediaType type,
                       const std::vector<anilist::title>& matches,
                       const std::vector<std::string>& mine) {
	std::vector<output::tableColumn> columns = {
	    {"#", true},
	    {"YEAR"},
	    {"TITLE", false, true},
	    {type == mediaType::kAnime ? "EPISODES" : "CHAPTERS", true},
	    {"SCORE", true},
	    {"YOURS"}};
	std::vector<std::vector<output::styled>> rows;
	for (size_t i = 0; i < matches.size(); ++i) {
		const auto& match = matches[i];
		rows.push_back({{std::to_string(i + 1), output::kCyan},
		                {match.year.empty() ? "-" : match.year},
		                {match.name},
		                {match.totalUnits > 0 ? std::to_string(match.totalUnits) : "?"},
		                {match.score > 0.0 ? fmt::format("{:.1f}", match.score) : "-"},
		                {mine[i], output::kGreen}});
	}
	return output::table(columns, rows, ctx.columns, ctx.color);
}

int runSearch(const command& self, const args::parsed& line, cliContext& ctx) {
	auto type = oneType(line);
	if (not type)
		return usageError(ctx, self, "give -a or -m, not both");
	auto query = readText(self, line, ctx);
	if (not query)
		return 2;
	if (query->empty())
		return usageError(ctx, self, "give the QUERY to search for");
	if (not load(ctx))
		return 1;
	auto found = anilist::search(ctx.post, *type, *query);
	if (not found.ok())
		return fail(ctx, webError(found.errorMessage()));
	const auto& matches = found.value();
	std::vector<std::string> mine;
	for (const auto& match : matches) {
		auto tracked = ctx.db.findByIds(*type, match.anilistId, match.malId);
		mine.push_back(tracked ? refs::of(*tracked) : "");
	}

	if (not line.has("json") and ctx.columns > 0) {
		if (matches.empty()) {
			ctx.err << fmt::format("AniList has no {} matching '{}'.\n", typeKey(*type), *query);
			return 0;
		}
		ctx.out << matchTable(ctx, *type, matches, mine);
		ctx.err << fmt::format("Add one with: {} add{} --pick=N QUERY\n", ctx.program,
		                       *type == mediaType::kManga ? " -m" : "");
		return 0;
	}
	std::vector<std::vector<output::cell>> rows;
	for (size_t i = 0; i < matches.size(); ++i) {
		const auto& match = matches[i];
		rows.push_back({output::number(static_cast<long long>(i) + 1), output::text(typeKey(*type)),
		                output::text(match.name),
		                match.year.empty() ? output::maybe("")
		                                   : output::cell{match.year, output::jsonForm::kNumber},
		                output::known(match.totalUnits), scoreCell(match.score),
		                output::known(match.anilistId), output::known(match.malId),
		                output::maybe(mine[i])});
	}
	writeRows(ctx, kSearchColumns, rows, line.has("json"), line.has("header"));
	return 0;
}

}  // namespace

std::vector<command> onlineCommands() {
	return {
	    {"add",
	     {},
	     "Add a title found on AniList",
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
	     {{'a', "", "", "add an anime (the default)"},
	      {'m', "", "", "add a manga"},
	      {0, "pick", "N", "take the Nth match without asking"},
	      {0, "status", "STATUS", "watching, completed, on-hold, dropped or planned",
	       statusWords()},
	      {0, "score", "N", "give it a score as well, 0 to 10"},
	      {0, "progress", "N", "the episodes or chapters done already"},
	      {0, "no-lookup", "", "add TITLE as written, without asking AniList"},
	      kQuiet},
	     "TITLE...",
	     runAdd,
	     operandKind::kText},
	    {"search",
	     {},
	     "Search AniList without adding anything",
	     "Prints up to ten matches for QUERY, anime unless -m is given, best\n"
	     "first. At a terminal they are drawn as a table. Into a pipe each is one\n"
	     "line of fields with a tab between them:\n"
	     "  pick type title year total score anilist_id mal_id ref\n"
	     "pick is the N of 'add --pick=N'; ref is the title in your lists, or '-'.\n"
	     "A year that is not known is '-', a total or a score that is not is 0.\n",
	     {{'a', "", "", "search anime (the default)"},
	      {'m', "", "", "search manga"},
	      {0, "json", "", "print one JSON object a line"},
	      {0, "header", "", "name the columns on the first line of a pipe"}},
	     "QUERY...",
	     runSearch,
	     operandKind::kText},
	};
}

}  // namespace catalist::cli
