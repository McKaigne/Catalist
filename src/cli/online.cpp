#include <fmt/format.h>

#include <algorithm>
#include <ostream>

#include "cli/common.hpp"
#include "cli/refs.hpp"
#include "core/text.hpp"

namespace catalist::cli {

namespace {

// The list `add` and `search` work in: anime unless -m says manga. Nothing
// when both -a and -m are given.
std::optional<mediaType> oneType(const args::parsed& line) {
	if (line.has("a") and line.has("m"))
		return std::nullopt;
	return line.has("m") ? mediaType::kManga : mediaType::kAnime;
}

// "28 episodes", or "? chapters" while the total is not known.
std::string lengthOf(const web::match& match, mediaType type) {
	return fmt::format("{} {}", match.totalUnits > 0 ? std::to_string(match.totalUnits) : "?",
	                   unitName(type, match.totalUnits));
}

// The match `add` is to take: the only one, the one --pick names, or the one
// chosen at a terminal. Nothing, with the reason on standard error, when
// there is no such match.
std::optional<web::match> chooseMatch(cliContext& ctx, web::iTracker& site, mediaType type,
                                      const std::string& query, std::optional<int> pick) {
	std::string name = providerName(site.site());
	auto found = site.search(type, query);
	if (not found.ok()) {
		fail(ctx, webError(found.errorMessage()));
		ctx.err << fmt::format("To add a title without {}: {} add --no-lookup TITLE\n", name,
		                       ctx.program);
		return std::nullopt;
	}
	const auto& matches = found.value();
	if (matches.empty()) {
		fail(ctx, fmt::format("{} has no {} matching '{}'", name, typeKey(type), query));
		return std::nullopt;
	}
	if (pick and static_cast<size_t>(*pick) > matches.size()) {
		fail(ctx, fmt::format(
		              "--pick={}, but {} has only {} for '{}'", *pick, name,
		              matches.size() == 1 ? "one match" : fmt::format("{} matches", matches.size()),
		              query));
		return std::nullopt;
	}
	if (pick)
		return matches[static_cast<size_t>(*pick) - 1];
	if (matches.size() == 1)
		return matches[0];

	ctx.err << fmt::format("{}'{}' matches {} {} on {}:\n", ctx.interactive ? "" : "error: ", query,
	                       matches.size(), typeKey(type), name);
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

// The title as the site has it: its name, the ids it knows, the totals and
// how much is out. Fails with what to tell the person.
result<entry> lookedUp(cliContext& ctx, web::iTracker& site, entry blank, const web::match& match) {
	blank.anilistId = match.anilistId;
	blank.malId = match.malId;
	blank.kitsuId = match.kitsuId;
	auto details = site.lookUp(blank);
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
	if (byHand and (pick or line.has("from")))
		return usageError(ctx, self,
		                  fmt::format("{} chooses among a site's matches; --no-lookup asks none",
		                              pick ? "--pick" : "--from"));
	if (not fromKnown(self, line, ctx))
		return 2;

	isoDate today = ctx.today();
	entry added;
	added.type = *type;
	added.title = *name;
	added.status = watchStatus::kPlanToWatch;
	added.updatedAt = today;
	// The site is asked before the catalogue is taken for writing: no other
	// program waits while the person chooses.
	if (not byHand) {
		if (asksProvider(line) and not load(ctx))
			return 1;
		auto site = trackerFor(ctx, siteOf(line, ctx));
		auto match = chooseMatch(ctx, *site, *type, added.title, pick);
		if (not match)
			return 1;
		auto found = lookedUp(ctx, *site, added, *match);
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
	if (auto mine = ctx.db.findByIds(*type, added.anilistId, added.malId, added.kitsuId)) {
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
const std::vector<std::string> kSearchColumns = {
    "pick", "type", "title", "year", "total", "score", "anilist_id", "mal_id", "ref", "kitsu_id"};

// The site's score out of 10, to one decimal; 0 while there is none.
output::cell scoreCell(double score) {
	if (score <= 0.0)
		return {"0", output::jsonForm::kNull};
	return {fmt::format("{:.1f}", score), output::jsonForm::kNumber};
}

// The matches as a table for a terminal.
std::string matchTable(const cliContext& ctx, mediaType type,
                       const std::vector<web::match>& matches,
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
	if (not fromKnown(self, line, ctx))
		return 2;
	if (not load(ctx))
		return 1;
	auto site = trackerFor(ctx, siteOf(line, ctx));
	auto found = site->search(*type, *query);
	if (not found.ok())
		return fail(ctx, webError(found.errorMessage()));
	const auto& matches = found.value();
	std::vector<std::string> mine;
	for (const auto& match : matches) {
		auto tracked = ctx.db.findByIds(*type, match.anilistId, match.malId, match.kitsuId);
		mine.push_back(tracked ? refs::of(*tracked) : "");
	}

	if (not line.has("json") and ctx.columns > 0) {
		if (matches.empty()) {
			ctx.err << fmt::format("{} has no {} matching '{}'.\n", providerName(site->site()),
			                       typeKey(*type), *query);
			return 0;
		}
		ctx.out << matchTable(ctx, *type, matches, mine);
		ctx.err << fmt::format("Add one with: {} add{}{} --pick=N QUERY\n", ctx.program,
		                       *type == mediaType::kManga ? " -m" : "",
		                       line.has("from") ? " --from=" + line.get("from") : "");
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
		                output::maybe(mine[i]), output::known(match.kitsuId)});
	}
	writeRows(ctx, kSearchColumns, rows, line.has("json"), line.has("header"));
	return 0;
}

// "a:1, m:3".
std::string joinedWith(const std::vector<std::string>& words, const std::string& between) {
	std::string all;
	for (const auto& word : words)
		all += (all.empty() ? "" : between) + word;
	return all;
}

// What refresh checks when no REF is given: the titles being followed.
bool followed(const entry& e) {
	return e.status == watchStatus::kWatching or e.status == watchStatus::kOnHold;
}

// The titles refresh is to check, read from the lists; nothing, with the
// exit code, when there are none to go on with.
struct refreshList {
	std::vector<entry> titles;
	std::optional<int> stop;
};

refreshList titlesToRefresh(const command& self, const args::parsed& line, cliContext& ctx) {
	refreshList wanted;
	if (not line.operands.empty()) {
		auto request = readRefs(self, line, ctx);
		if (request.stop) {
			wanted.stop = request.stop;
			return wanted;
		}
		auto found = load(ctx) ? resolve(ctx, request.refs, request.how) : std::nullopt;
		if (not found)
			wanted.stop = 1;
		else
			wanted.titles = *found;
		return wanted;
	}
	if (not load(ctx)) {
		wanted.stop = 1;
		return wanted;
	}
	auto only = onlyType(line);
	for (mediaType type : {mediaType::kAnime, mediaType::kManga})
		for (const auto& e : ctx.db.getEntries(type))
			if ((not only or *only == type) and followed(e))
				wanted.titles.push_back(e);
	return wanted;
}

int runRefresh(const command& self, const args::parsed& line, cliContext& ctx) {
	if (not fromKnown(self, line, ctx))
		return 2;
	auto wanted = titlesToRefresh(self, line, ctx);
	if (wanted.stop)
		return *wanted.stop;
	bool quiet = line.has("q");
	if (wanted.titles.empty()) {
		if (not quiet)
			ctx.err << "Nothing is being watched or read: name the titles to refresh.\n";
		return 0;
	}

	// The site is asked before the catalogue is taken for writing, as by
	// add: nothing waits on the network.
	auto site = trackerFor(ctx, siteOf(line, ctx));
	std::string name = providerName(site->site());
	if (not quiet and wanted.titles.size() > 1 and site->site() != provider::kAniList)
		ctx.err << fmt::format("Asking {} about {}, one a second.\n", name,
		                       titles(wanted.titles.size()));
	auto answers = site->lookUpMany(wanted.titles);
	std::vector<std::string> unknown;
	for (size_t i = 0; i < answers.size(); ++i) {
		if (answers[i].ok())
			continue;
		if (web::unreachable(answers[i].errorMessage()))
			return fail(ctx, webError(answers[i].errorMessage()));
		unknown.push_back(refs::of(wanted.titles[i]));
	}

	if (not loadForWrite(ctx))
		return 1;
	isoDate today = ctx.today();
	std::vector<journal::change> changes;
	size_t fresh = 0;
	for (size_t i = 0; i < answers.size(); ++i) {
		const entry& asked = wanted.titles[i];
		auto now = answers[i].ok() ? ctx.db.getById(asked.type, asked.id) : std::nullopt;
		// Removed meanwhile by another program: nothing to refresh.
		if (not now)
			continue;
		entry after = applyWebFacts(*now, answers[i].value().facts, today);
		ctx.db.updateEntry(after);
		changes.push_back({after.type, after.id, *now, after});
		fresh += newReleases(after) > 0 ? 1 : 0;
	}
	// What is out and the ids are kept, but only a new total is a step for
	// undo, as for import.
	bool shown = std::any_of(changes.begin(), changes.end(), [](const journal::change& made) {
		return not journal::changedFields(made).empty();
	});
	if (shown) {
		int code = saveAndReport(ctx, line, changes);
		if (code != 0)
			return code;
	} else if (not ctx.db.saveAll()) {
		return saveFailed(ctx);
	}
	if (quiet)
		return 0;
	if (not unknown.empty())
		ctx.err << fmt::format("note: {} could not find {}.\n", name, joinedWith(unknown, ", "));
	ctx.err << fmt::format("Checked {} on {}{}.\n", titles(changes.size()), name,
	                       fresh == 0
	                           ? ""
	                           : fmt::format("; {} new episodes or chapters out",
	                                         fresh == 1 ? "1 has" : fmt::format("{} have", fresh)));
	return 0;
}

}  // namespace

std::vector<command> onlineCommands() {
	return {
	    {"add",
	     {},
	     "Add a title found on AniList, MyAnimeList or Kitsu",
	     std::string("Searches your provider (see 'provider'), or the site --from names, for\n"
	                 "TITLE, an anime unless -m is given, and adds the match: the only one,\n"
	                 "the one you choose when there are several, or the one --pick names by\n"
	                 "its number in that list ('search' prints the same list). Where nobody\n"
	                 "can be asked, several matches without --pick are an error. A new title\n"
	                 "is planned, or watching once it has a --progress, unless --status says\n"
	                 "otherwise. A title that is in your list already is left as it is.\n\n"
	                 "With --no-lookup no site is asked: TITLE is added as it is written,\n"
	                 "without ids or totals ('set' can give it a total).\n\n") +
	         kChangeFormat,
	     {{'a', "", "", "add an anime (the default)"},
	      {'m', "", "", "add a manga"},
	      kFrom,
	      {0, "pick", "N", "take the Nth match without asking"},
	      {0, "status", "STATUS", "watching, completed, on-hold, dropped or planned",
	       statusWords()},
	      {0, "score", "N", "give it a score as well, 0 to 10"},
	      {0, "progress", "N", "the episodes or chapters done already"},
	      {0, "no-lookup", "", "add TITLE as written, without asking a site"},
	      kQuiet},
	     "TITLE...",
	     runAdd,
	     operandKind::kText},
	    {"search",
	     {},
	     "Search a site without adding anything",
	     "Prints up to ten matches for QUERY on your provider, or the site --from\n"
	     "names, anime unless -m is given, best first. At a terminal they are\n"
	     "drawn as a table. Into a pipe each is one line of fields with a tab\n"
	     "between them:\n"
	     "  pick type title year total score anilist_id mal_id ref kitsu_id\n"
	     "pick is the N of 'add --pick=N'; ref is the title in your lists, or '-'.\n"
	     "A year that is not known is '-', a total, a score or an id that is not\n"
	     "is 0. Each site knows its own ids; AniList also MyAnimeList's, and Kitsu\n"
	     "both of the others'.\n",
	     {{'a', "", "", "search anime (the default)"},
	      {'m', "", "", "search manga"},
	      kFrom,
	      {0, "json", "", "print one JSON object a line"},
	      {0, "header", "", "name the columns on the first line of a pipe"}},
	     "QUERY...",
	     runSearch,
	     operandKind::kText},
	    {"refresh",
	     {},
	     "Ask a site what is out of the titles you follow",
	     std::string("Asks your provider, or the site --from names, for each title's totals\n"
	                 "and how many of its episodes or chapters are out (what the new column\n"
	                 "of ls counts from), and fills in the ids that site knows. Without a\n"
	                 "REF it asks about the titles you are watching or reading, or have on\n"
	                 "hold. AniList answers fifty titles a request; MyAnimeList and Kitsu\n"
	                 "one a second, and only AniList says how much of a running show has\n"
	                 "aired. A changed total is a step for undo.\n\n") +
	         kRefsHelp + "\n" + kChangeFormat,
	     withRefOptions({kFrom, kQuiet}),
	     "[REF...]",
	     runRefresh,
	     operandKind::kRefs},
	};
}

}  // namespace catalist::cli
