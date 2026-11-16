#include <fmt/format.h>

#include <algorithm>
#include <map>
#include <ostream>
#include <sstream>

#include "cli/common.hpp"
#include "cli/refs.hpp"

namespace catalist::cli {

namespace {

using output::paint;

// What `show` adds to the columns of a title (kColumns).
const std::vector<std::string> kShowColumns = {"total_volumes", "released", "anilist_id",
                                               "mal_id",        "url",      "kitsu_id"};

std::vector<output::cell> showCellsOf(const entry& e, provider site) {
	auto cells = cellsOf(e);
	for (const auto& more :
	     {output::known(e.totalVolumes), output::known(e.released), output::known(e.anilistId),
	      output::known(e.malId), output::maybe(pageOn(e, site)), output::known(e.kitsuId)})
		cells.push_back(more);
	return cells;
}

const char* const kRowFormat =
    "At a terminal the titles are drawn as a table. Into a pipe or a file each\n"
    "title is one line of fields with a tab between them, so cut, awk and\n"
    "read can take them apart:\n"
    "  ref type title status score progress total volumes rewatched\n"
    "  started finished updated new\n"
    "A date that is not set is '-' there, and a total that is not known is 0.\n";

// The list as a table for a terminal.
std::string listTable(const cliContext& ctx, const std::vector<entry>& shown) {
	std::vector<output::tableColumn> columns = {{"REF"},         {"TITLE", false, true}, {"STATUS"},
	                                            {"SCORE", true}, {"PROGRESS"},           {"NEW"}};
	std::vector<std::vector<output::styled>> rows;
	for (const auto& e : shown) {
		int fresh = newReleases(e);
		rows.push_back({{refs::of(e), output::kCyan},
		                {e.title},
		                {statusToString(e.status, e.type), statusColor(e.status)},
		                {e.score > 0 ? std::to_string(e.score) : "-"},
		                {outOf(e.progress, e.totalUnits)},
		                {fresh > 0 ? fmt::format("+{}", fresh) : "", output::kGreen}});
	}
	return output::table(columns, rows, ctx.columns, ctx.color);
}

const std::map<std::string, sortKey> kSortKeys = {
    {"new", sortKey::kNew},       {"updated", sortKey::kUpdated}, {"score", sortKey::kScore},
    {"status", sortKey::kStatus}, {"title", sortKey::kTitle},     {"progress", sortKey::kProgress},
    {"date", sortKey::kDate}};

std::vector<std::string> sortWords() {
	std::vector<std::string> words;
	for (const auto& [word, key] : kSortKeys)
		words.push_back(word);
	return words;
}

int runList(const command& self, const args::parsed& line, cliContext& ctx) {
	auto query = readText(self, line, ctx);
	if (not query)
		return 2;
	// -s watching,on-hold: any of them; no -s, all of them.
	std::vector<watchStatus> statuses;
	std::istringstream given(line.get("s"));
	for (std::string word; std::getline(given, word, ',');) {
		auto status = parseStatus(word);
		if (not status)
			return usageError(ctx, self, fmt::format("unknown status '{}'", word));
		statuses.push_back(*status);
	}
	if (line.has("s") and statuses.empty())
		return usageError(ctx, self, "-s needs a status");
	std::optional<provider> unlinked;
	if (line.has("unlinked")) {
		unlinked = parseProvider(line.get("unlinked"));
		if (not unlinked)
			return usageError(
			    ctx, self,
			    fmt::format("--unlinked '{}' is not anilist, mal or kitsu", line.get("unlinked")));
	}
	sortKey key = sortKey::kTitle;
	if (line.has("sort")) {
		auto found = kSortKeys.find(line.get("sort"));
		if (found == kSortKeys.end())
			return usageError(ctx, self, fmt::format("unknown sort key '{}'", line.get("sort")));
		key = found->second;
	}
	if (not load(ctx))
		return 1;

	auto only = onlyType(line);
	std::vector<entry> shown;
	for (mediaType type : {mediaType::kAnime, mediaType::kManga}) {
		if (only and *only != type)
			continue;
		for (const auto& e : ctx.db.getEntries(type))
			if ((statuses.empty() or
			     std::find(statuses.begin(), statuses.end(), e.status) != statuses.end()) and
			    refs::titleHolds(e, *query) and (not unlinked or idOn(e, *unlinked) == 0))
				shown.push_back(e);
	}
	catalogue::sortEntries(shown, key, line.has("r"));

	if (line.has("q")) {
		for (const auto& e : shown)
			ctx.out << refs::of(e) << "\n";
		return 0;
	}
	if (not line.has("json") and ctx.columns > 0) {
		if (shown.empty())
			ctx.err << "No titles to list.\n";
		else
			ctx.out << listTable(ctx, shown);
		return 0;
	}
	std::vector<std::vector<output::cell>> rows;
	for (const auto& e : shown)
		rows.push_back(cellsOf(e));
	writeRows(ctx, kColumns, rows, line.has("json"), line.has("header"));
	return 0;
}

// One labelled line of a card, the labels `width` columns wide.
void cardRow(cliContext& ctx, const std::string& label, const std::string& value, int width = 13) {
	ctx.out << "  " << paint(fmt::format("{:<{}}", label, width), output::kDim, ctx.color) << value
	        << "\n";
}

// Everything about one title, for a terminal.
void printCard(cliContext& ctx, const entry& e) {
	auto row = [&ctx](const std::string& label, const std::string& value) {
		cardRow(ctx, label, value);
	};
	auto idText = [](int id) { return id > 0 ? std::to_string(id) : std::string("-"); };
	auto dateText = [](const std::optional<isoDate>& day) {
		return day ? day->toString() : std::string("-");
	};
	std::string page = pageOn(e, ctx.db.getProvider());
	int fresh = newReleases(e);

	ctx.out << paint(e.title, output::kBold, ctx.color) << "\n";
	row("Ref", paint(refs::of(e), output::kCyan, ctx.color));
	row("Type", typeToString(e.type));
	row("AniList", idText(e.anilistId));
	row("MyAnimeList", idText(e.malId));
	row("Kitsu", idText(e.kitsuId));
	if (not page.empty())
		row("Page", page);
	row("Status", paint(statusToString(e.status, e.type), statusColor(e.status), ctx.color));
	row("Score", e.score > 0 ? fmt::format("{}/10", e.score) : "-");
	row("Progress",
	    fmt::format("{} {}", outOf(e.progress, e.totalUnits), unitName(e.type)) +
	        (fresh > 0 ? paint(fmt::format("  (+{} new)", fresh), output::kGreen, ctx.color) : ""));
	if (e.type == mediaType::kManga)
		row("Volumes", outOf(e.volumes, e.totalVolumes));
	row(e.type == mediaType::kAnime ? "Rewatched" : "Reread", std::to_string(e.rewatched));
	row("Started", dateText(e.startDate));
	row("Finished", dateText(e.finishDate));
	row("Updated", dateText(e.updatedAt));
}

int runShow(const command& self, const args::parsed& line, cliContext& ctx) {
	auto request = readRefs(self, line, ctx);
	if (request.stop)
		return *request.stop;
	if (not load(ctx))
		return 1;
	auto found = resolve(ctx, request.refs, request.how);
	if (not found)
		return 1;

	if (not line.has("json") and ctx.columns > 0) {
		for (size_t i = 0; i < found->size() and ctx.out; ++i) {
			ctx.out << (i > 0 ? "\n" : "");
			printCard(ctx, (*found)[i]);
		}
		return 0;
	}
	std::vector<std::string> names = kColumns;
	names.insert(names.end(), kShowColumns.begin(), kShowColumns.end());
	std::vector<std::vector<output::cell>> rows;
	for (const auto& e : *found)
		rows.push_back(showCellsOf(e, ctx.db.getProvider()));
	writeRows(ctx, names, rows, line.has("json"), line.has("header"));
	return 0;
}

// The columns of a list's numbers: how many titles, how many in each
// status, the mean of the scores given, the episodes or chapters done
// (units), the volumes read, the rewatches, and the days all of it took.
const std::vector<std::string> kStatsColumns = {"type",    "total",   "watching",  "completed",
                                                "on_hold", "dropped", "planned",   "mean",
                                                "units",   "volumes", "rewatched", "days"};

std::vector<output::cell> statsCells(const catalogue& db, mediaType type) {
	std::vector<output::cell> cells = {
	    output::text(typeKey(type)),
	    output::number(static_cast<long long>(db.getEntries(type).size()))};
	for (watchStatus status : kStatuses)
		cells.push_back(output::number(static_cast<long long>(db.countByStatus(type, status))));
	double mean = db.averageScore(type);
	cells.push_back(mean > 0.0
	                    ? output::cell{fmt::format("{:.2f}", mean), output::jsonForm::kNumber}
	                    : output::cell{"0", output::jsonForm::kNull});
	cells.push_back(output::number(db.totalProgress(type)));
	cells.push_back(output::number(db.totalVolumes(type)));
	cells.push_back(output::number(db.totalRewatched(type)));
	cells.push_back({fmt::format("{:.1f}", db.daysSpent(type)), output::jsonForm::kNumber});
	return cells;
}

// The numbers of one list, for a terminal.
void printStatsCard(cliContext& ctx, mediaType type) {
	// Wide enough for "Plan to Watch".
	constexpr int kLabelWidth = 15;
	const catalogue& db = ctx.db;
	bool anime = type == mediaType::kAnime;
	double mean = db.averageScore(type);
	auto row = [&ctx](const std::string& label, const std::string& value) {
		cardRow(ctx, label, value, kLabelWidth);
	};
	ctx.out << paint(typeToString(type), output::kBold, ctx.color) << "\n";
	row("Titles", std::to_string(db.getEntries(type).size()));
	for (watchStatus status : kStatuses)
		row(statusToString(status, type), std::to_string(db.countByStatus(type, status)));
	row("Mean score", mean > 0.0 ? fmt::format("{:.2f}", mean) : "-");
	row(anime ? "Episodes" : "Chapters", std::to_string(db.totalProgress(type)));
	if (not anime)
		row("Volumes", std::to_string(db.totalVolumes(type)));
	row(anime ? "Rewatched" : "Reread", std::to_string(db.totalRewatched(type)));
	row("Days spent", fmt::format("{:.1f}", db.daysSpent(type)));
}

int runStats(const command& self, const args::parsed& line, cliContext& ctx) {
	if (not line.operands.empty())
		return usageError(ctx, self, "stats takes no operands");
	if (not load(ctx))
		return 1;
	auto only = onlyType(line);
	std::vector<mediaType> types;
	for (mediaType type : {mediaType::kAnime, mediaType::kManga})
		if (not only or *only == type)
			types.push_back(type);

	if (not line.has("json") and ctx.columns > 0) {
		for (size_t i = 0; i < types.size() and ctx.out; ++i) {
			ctx.out << (i > 0 ? "\n" : "");
			printStatsCard(ctx, types[i]);
		}
		return 0;
	}
	std::vector<std::vector<output::cell>> rows;
	for (mediaType type : types)
		rows.push_back(statsCells(ctx.db, type));
	writeRows(ctx, kStatsColumns, rows, line.has("json"), line.has("header"));
	return 0;
}

}  // namespace

std::vector<command> listingCommands() {
	return {
	    {"ls",
	     {},
	     "List your titles",
	     std::string("Lists both lists in title order. QUERY keeps the titles that hold its\n"
	                 "words, in capitals or not; they need no quotes. -s takes several\n"
	                 "statuses with commas between them: -s watching,on-hold.\n\n") +
	         kRowFormat,
	     {{'a', "", "", "list anime only"},
	      {'m', "", "", "list manga only"},
	      {'s', "", "STATUS", "only watching, completed, on-hold, dropped, planned, or a,b",
	       statusWords()},
	      {0, "sort", "KEY", "new, updated, score, progress, date, status or title", sortWords()},
	      {'r', "", "", "reverse the order"},
	      {0,
	       "unlinked",
	       "SITE",
	       "only titles with no id on the site",
	       {"anilist", "mal", "kitsu"}},
	      {'q', "", "", "print the refs and nothing else"},
	      {0, "json", "", "print one JSON object a line"},
	      {0, "header", "", "name the columns on the first line of a pipe"}},
	     "[QUERY...]",
	     runList,
	     operandKind::kTitle},
	    {"show",
	     {},
	     "Show titles in full",
	     "A REF is a ref such as a:12, or part of a title; a REF of - reads REFs\n"
	     "from standard input, one a line. Into a pipe a title is one line: the\n"
	     "columns of ls, then total_volumes, released, anilist_id, mal_id, url\n"
	     "and kitsu_id. url is the title's page on your provider.\n",
	     withRefOptions({{0, "json", "", "print one JSON object a line"},
	                     {0, "header", "", "name the columns on the first line of a pipe"}}),
	     "REF...",
	     runShow,
	     operandKind::kRefs},
	    {"stats",
	     {},
	     "Show the numbers of your lists",
	     "Into a pipe each list is one line of fields with a tab between them:\n"
	     "  type total watching completed on_hold dropped planned mean units\n"
	     "  volumes rewatched days\n"
	     "mean is the mean of the scores you gave (0 when there is none), units\n"
	     "the episodes or chapters done, and days an estimate of the time they\n"
	     "took: 24 minutes an episode, 5 minutes a chapter.\n",
	     {{'a', "", "", "the anime list only"},
	      {'m', "", "", "the manga list only"},
	      {0, "json", "", "print one JSON object a line"},
	      {0, "header", "", "name the columns on the first line of a pipe"}},
	     "",
	     runStats},
	};
}

}  // namespace catalist::cli
