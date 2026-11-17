#include <algorithm>
#include <format>
#include <map>
#include <ostream>
#include <sstream>

#include "cli/common.hpp"
#include "cli/refs.hpp"
#include "core/lists.hpp"

namespace catalist::cli {

namespace {

using output::Paint;

const char* const kRowFormat =
    "At a terminal the titles are drawn as a table. Into a pipe or a file each\n"
    "title is one line of fields with a tab between them, so cut, awk and\n"
    "read can take them apart:\n"
    "  ref type title status score progress total volumes rewatched\n"
    "  started finished updated new\n"
    "A date that is not set is '-' there, and a total that is not known is 0.\n";

// The list as a table for a terminal.
std::string ListTable(const CliContext& ctx, const std::vector<Entry>& shown) {
	std::vector<output::TableColumn> columns = {{"REF"},   {"TITLE", false, true}, {"STATUS"},
	                                            {"SCORE"}, {"PROGRESS"},           {"NEW"}};
	std::vector<std::vector<output::Styled>> rows;
	for (const auto& e : shown) {
		int fresh = NewReleases(e);
		rows.push_back({{refs::Of(e), output::kCyan},
		                {e.title},
		                {StatusToString(e.status, e.type), StatusColor(e.status)},
		                {output::Stars(e.score), output::kYellow},
		                {OutOf(e.progress, e.total_units)},
		                {fresh > 0 ? std::format("+{}", fresh) : "", output::kGreen}});
	}
	return output::Table(columns, rows, ctx.columns, ctx.color);
}

const std::map<std::string, SortKey> kSortKeys = {
    {"new", SortKey::kNew},       {"updated", SortKey::kUpdated}, {"score", SortKey::kScore},
    {"status", SortKey::kStatus}, {"title", SortKey::kTitle},     {"progress", SortKey::kProgress},
    {"date", SortKey::kDate}};

std::vector<std::string> SortWords() {
	std::vector<std::string> words;
	for (const auto& [word, key] : kSortKeys)
		words.push_back(word);
	return words;
}

int RunList(const Command& self, const args::Parsed& line, CliContext& ctx) {
	auto text = ReadText(self, line, ctx);
	if (not text)
		return 2;
	ListQuery query{.text = *text,
	                .only = OnlyType(line),
	                .reverse = line.Has("r"),
	                .refs_only = line.Has("q"),
	                .json = line.Has("json"),
	                .header = line.Has("header")};
	// -s watching,on-hold: any of them; no -s, all of them.
	std::istringstream given(line.Get("s"));
	for (std::string word; std::getline(given, word, ',');) {
		auto status = StatusNamed(word);
		if (not status)
			return UsageError(
			    ctx, self,
			    std::format("unknown status '{}'{}", word, DidYouMean(word, StatusWords())));
		query.statuses.push_back(*status);
	}
	if (line.Has("s") and query.statuses.empty())
		return UsageError(ctx, self, "-s needs a status");
	// --linked anilist,mal and --unlinked kitsu: on these sites, and not on
	// those.
	for (const char* option : {"linked", "unlinked"}) {
		std::istringstream sites(line.Get(option));
		auto& into = std::string(option) == "linked" ? query.linked : query.unlinked;
		for (std::string word; std::getline(sites, word, ',');) {
			auto site = ParseProvider(word);
			if (not site)
				return UsageError(ctx, self,
				                  std::format("--{} '{}' is not anilist, mal or kitsu{}", option,
				                              word, DidYouMean(word, {"anilist", "mal", "kitsu"})));
			into.push_back(*site);
		}
		if (line.Has(option) and into.empty())
			return UsageError(ctx, self, std::format("--{} needs a site", option));
	}
	if (line.Has("sort")) {
		auto found = kSortKeys.find(line.Get("sort"));
		if (found == kSortKeys.end())
			return UsageError(ctx, self, std::format("unknown sort key '{}'", line.Get("sort")));
		query.key = found->second;
	}
	return PrintList(ctx, query);
}

// One labelled line of a card, the labels `width` columns wide.
void CardRow(CliContext& ctx, const std::string& label, const std::string& value, int width = 13) {
	ctx.out << "  " << Paint(std::format("{:<{}}", label, width), output::kDim, ctx.color) << value
	        << "\n";
}

// Everything about one title, for a terminal.
void PrintCard(CliContext& ctx, const Entry& e) {
	auto row = [&ctx](const std::string& label, const std::string& value) {
		CardRow(ctx, label, value);
	};
	auto id_text = [](int id) { return id > 0 ? std::to_string(id) : std::string("-"); };
	auto date_text = [](const std::optional<IsoDate>& day) {
		return day ? day->ToString() : std::string("-");
	};
	std::string page = PageOn(e, ctx.db.provider());
	int fresh = NewReleases(e);

	ctx.out << Paint(e.title, output::kBold, ctx.color) << "\n";
	row("Ref", Paint(refs::Of(e), output::kCyan, ctx.color));
	row("Type", TypeToString(e.type));
	row("AniList", id_text(e.anilist_id));
	row("MyAnimeList", id_text(e.mal_id));
	row("Kitsu", id_text(e.kitsu_id));
	if (not page.empty())
		row("Page", page);
	row("Status", Paint(StatusToString(e.status, e.type), StatusColor(e.status), ctx.color));
	row("Score",
	    e.score > 0
	        ? std::format("{}  {}/10", Paint(output::Stars(e.score), output::kYellow, ctx.color),
	                      e.score)
	        : "-");
	row("Progress",
	    std::format("{} {}", OutOf(e.progress, e.total_units), UnitName(e.type)) +
	        (fresh > 0 ? Paint(std::format("  (+{} new)", fresh), output::kGreen, ctx.color) : ""));
	if (e.type == MediaType::kManga)
		row("Volumes", OutOf(e.volumes, e.total_volumes));
	row(e.type == MediaType::kAnime ? "Rewatched" : "Reread", std::to_string(e.rewatched));
	row("Started", date_text(e.start_date));
	row("Finished", date_text(e.finish_date));
	row("Updated", date_text(e.updated_at));
}

int RunShow(const Command& self, const args::Parsed& line, CliContext& ctx) {
	auto request = ReadRefs(self, line, ctx);
	if (request.stop)
		return *request.stop;
	if (not Load(ctx))
		return 1;
	auto found = Resolve(ctx, request.refs, request.how);
	if (not found)
		return 1;

	if (not line.Has("json") and ctx.columns > 0) {
		for (size_t i = 0; i < found->size() and ctx.out; ++i) {
			ctx.out << (i > 0 ? "\n" : "");
			PrintCard(ctx, (*found)[i]);
		}
		return 0;
	}
	std::vector<std::vector<output::Cell>> rows;
	for (const auto& e : *found)
		rows.push_back(FullCellsOf(e, ctx.db.provider()));
	WriteRows(ctx, AllColumns(), rows, line.Has("json"), line.Has("header"));
	return 0;
}

// The columns of a list's numbers: how many titles, how many in each
// status, the mean of the scores given, the episodes or chapters done
// (units), the volumes read, the rewatches, the days all of it took, and
// how many are being rewatched (a later column, so it comes last).
const std::vector<std::string> kStatsColumns = {
    "type", "total", "watching", "completed", "on_hold", "dropped",   "planned",
    "mean", "units", "volumes",  "rewatched", "days",    "rewatching"};

std::vector<output::Cell> StatsCells(const std::vector<Entry>& list, MediaType type) {
	std::vector<output::Cell> cells = {output::Text(TypeKey(type)),
	                                   output::Number(static_cast<long long>(list.size()))};
	auto counted = [&list](WatchStatus status) {
		return output::Number(static_cast<long long>(CountByStatus(list, status)));
	};
	for (WatchStatus status : kStatuses)
		if (status != WatchStatus::kRewatching)
			cells.push_back(counted(status));
	double mean = AverageScore(list);
	cells.push_back(mean > 0.0
	                    ? output::Cell{std::format("{:.2f}", mean), output::JsonForm::kNumber}
	                    : output::Cell{"0", output::JsonForm::kNull});
	cells.push_back(output::Number(TotalProgress(list)));
	cells.push_back(output::Number(TotalVolumes(list)));
	cells.push_back(output::Number(TotalRewatched(list)));
	cells.push_back({std::format("{:.1f}", DaysSpent(list)), output::JsonForm::kNumber});
	cells.push_back(counted(WatchStatus::kRewatching));
	return cells;
}

// The numbers of one list, for a terminal.
void PrintStatsCard(CliContext& ctx, const std::vector<Entry>& list, MediaType type) {
	// Wide enough for "Plan to Watch".
	constexpr int kLabelWidth = 15;
	bool anime = type == MediaType::kAnime;
	double mean = AverageScore(list);
	auto row = [&ctx](const std::string& label, const std::string& value) {
		CardRow(ctx, label, value, kLabelWidth);
	};
	ctx.out << Paint(TypeToString(type), output::kBold, ctx.color) << "\n";
	row("Titles", std::to_string(list.size()));
	for (WatchStatus status : kStatuses)
		row(StatusToString(status, type), std::to_string(CountByStatus(list, status)));
	row("Mean score", mean > 0.0 ? std::format("{:.2f}", mean) : "-");
	row(anime ? "Episodes" : "Chapters", std::to_string(TotalProgress(list)));
	if (not anime)
		row("Volumes", std::to_string(TotalVolumes(list)));
	row(anime ? "Rewatched" : "Reread", std::to_string(TotalRewatched(list)));
	row("Days spent", std::format("{:.1f}", DaysSpent(list)));
}

int RunStats(const Command& self, const args::Parsed& line, CliContext& ctx) {
	if (not line.operands.empty())
		return UsageError(ctx, self, "stats takes no operands");
	if (not Load(ctx))
		return 1;
	auto only = OnlyType(line);
	std::vector<MediaType> types;
	for (MediaType type : {MediaType::kAnime, MediaType::kManga})
		if (not only or *only == type)
			types.push_back(type);

	if (not line.Has("json") and ctx.columns > 0) {
		for (size_t i = 0; i < types.size() and ctx.out; ++i) {
			ctx.out << (i > 0 ? "\n" : "");
			PrintStatsCard(ctx, ctx.db.GetEntries(types[i]), types[i]);
		}
		return 0;
	}
	std::vector<std::vector<output::Cell>> rows;
	for (MediaType type : types)
		rows.push_back(StatsCells(ctx.db.GetEntries(type), type));
	WriteRows(ctx, kStatsColumns, rows, line.Has("json"), line.Has("header"));
	return 0;
}

}  // namespace

int PrintList(CliContext& ctx, const ListQuery& query) {
	if (not Load(ctx))
		return 1;
	std::vector<Entry> shown;
	for (MediaType type : {MediaType::kAnime, MediaType::kManga}) {
		if (query.only and *query.only != type)
			continue;
		for (const auto& e : ctx.db.GetEntries(type))
			if ((query.statuses.empty() or std::ranges::contains(query.statuses, e.status)) and
			    refs::TitleHolds(e, query.text) and
			    std::ranges::all_of(query.linked,
			                        [&e](Provider site) { return IdOn(e, site) > 0; }) and
			    std::ranges::all_of(query.unlinked,
			                        [&e](Provider site) { return IdOn(e, site) == 0; }))
				shown.push_back(e);
	}
	SortEntries(shown, query.key, query.reverse);

	if (query.refs_only) {
		for (const auto& e : shown)
			ctx.out << refs::Of(e) << "\n";
		return 0;
	}
	if (not query.json and ctx.columns > 0) {
		if (shown.empty())
			ctx.err << "No titles to list.\n";
		else
			ctx.out << ListTable(ctx, shown);
		return 0;
	}
	std::vector<std::vector<output::Cell>> rows;
	for (const auto& e : shown)
		rows.push_back(CellsOf(e));
	WriteRows(ctx, kColumns, rows, query.json, query.header);
	return 0;
}

std::vector<Command> ListingCommands() {
	return {
	    {.name = "ls",
	     .summary = "List your titles",
	     .details =
	         std::string("Lists both lists in title order. QUERY keeps the titles that hold its\n"
	                     "words, in capitals or not; they need no quotes. -s takes several\n"
	                     "statuses with commas between them: -s watching,on-hold.\n\n") +
	         kRowFormat,
	     .options = {{'a', "", "", "list anime only"},
	                 {'m', "", "", "list manga only"},
	                 {'s', "", "STATUS",
	                  "only watching, completed, on-hold, dropped, planned, rewatching, or a,b",
	                  StatusWords()},
	                 {0, "sort", "KEY", "new, updated, score, progress, date, status or title",
	                  SortWords()},
	                 {'r', "", "", "reverse the order"},
	                 {0,
	                  "linked",
	                  "SITE",
	                  "only titles linked to the site, or to each of a,b",
	                  {"anilist", "mal", "kitsu"}},
	                 {0,
	                  "unlinked",
	                  "SITE",
	                  "only titles not linked to the site, nor to any of a,b",
	                  {"anilist", "mal", "kitsu"}},
	                 {'q', "", "", "print the refs and nothing else"},
	                 {0, "json", "", "print one JSON object a line"},
	                 {0, "header", "", "name the columns on the first line of a pipe"}},
	     .operands = "[QUERY...]",
	     .run = RunList,
	     .completes = OperandKind::kTitle},
	    {.name = "show",
	     .summary = "Show titles in full",
	     .details = "A REF is a ref such as a:12, or part of a title; a REF of - reads REFs\n"
	                "from standard input, one a line. Into a pipe a title is one line: the\n"
	                "columns of ls, then total_volumes, released, anilist_id, mal_id, url\n"
	                "and kitsu_id. url is the title's page on your provider.\n",
	     .options =
	         WithRefOptions({{0, "json", "", "print one JSON object a line"},
	                         {0, "header", "", "name the columns on the first line of a pipe"}}),
	     .operands = "REF...",
	     .run = RunShow,
	     .completes = OperandKind::kRefs},
	    {.name = "stats",
	     .summary = "Show the numbers of your lists",
	     .details = "Into a pipe each list is one line of fields with a tab between them:\n"
	                "  type total watching completed on_hold dropped planned mean units\n"
	                "  volumes rewatched days rewatching\n"
	                "mean is the mean of the scores you gave (0 when there is none), units\n"
	                "the episodes or chapters done, and days an estimate of the time they\n"
	                "took: 24 minutes an episode, 5 minutes a chapter.\n",
	     .options = {{'a', "", "", "the anime list only"},
	                 {'m', "", "", "the manga list only"},
	                 {0, "json", "", "print one JSON object a line"},
	                 {0, "header", "", "name the columns on the first line of a pipe"}},
	     .run = RunStats},
	};
}

}  // namespace catalist::cli
