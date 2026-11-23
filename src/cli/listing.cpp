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
    "  started finished updated\n"
    "A date that is not set is '-' there, and a total that is not known is 0.\n";

std::string ListTable(const CliContext& ctx, const std::vector<Entry>& shown) {
	std::vector<output::TableColumn> columns = {{"", false, true}, {""}, {""}, {""}};
	std::vector<output::Section> sections = {{"ANIME", {}}, {"MANGA", {}}};
	for (const auto& e : shown) {
		sections[e.type == MediaType::kAnime ? 0 : 1].rows.push_back(
		    {{DisplayName(e)},
		     {StatusToString(e.status, e.type), StatusColor(e.status)},
		     {output::Stars(e.score), output::kYellow},
		     {OutOf(e.progress, e.total_units)}});
	}
	return output::Sections(columns, sections, ctx.columns, ctx.color);
}

void ShowTable(CliContext& ctx, const std::string& table) {
	auto lines = static_cast<int>(std::ranges::count(table, '\n'));
	if (ctx.pager and ctx.rows > 0 and lines >= ctx.rows) {
		ctx.out.flush();
		if (ctx.pager(table))
			return;
	}
	ctx.out << table;
}

void SayNothingToList(CliContext& ctx, const ListQuery& query) {
	bool empty = ctx.db.GetEntries(MediaType::kAnime).empty() and
	             ctx.db.GetEntries(MediaType::kManga).empty();
	if (not query.is_home)
		ctx.err << "No titles to list.\n";
	else if (empty)
		ctx.err << "Nothing here yet. Add a title with 'catalist add NAME', or bring in a list\n"
		           "with 'catalist import FILE'.\n";
	else
		ctx.err << "Nothing in progress. See your lists with 'catalist ls', or pick one up with\n"
		           "'catalist resume'.\n";
}

const std::map<std::string, SortKey> kSortKeys = {
    {"updated", SortKey::kUpdated}, {"score", SortKey::kScore},       {"status", SortKey::kStatus},
    {"title", SortKey::kTitle},     {"progress", SortKey::kProgress}, {"date", SortKey::kDate}};

int RunList(const Command& self, const args::Parsed& line, CliContext& ctx) {
	ListQuery query{.text = ReadText(line),
	                .only = OnlyType(line),
	                .reverse = line.Has("r"),
	                .refs_only = line.Has("q")};
	// -s watching,on-hold: any of them; no -s, all of them.
	std::istringstream given(line.Get("s"));
	for (std::string word; std::getline(given, word, ',');) {
		auto status = StatusNamed(word);
		if (not status)
			return UsageError(ctx, self, std::format("unknown status '{}'", word));
		query.statuses.push_back(*status);
	}
	if (line.Has("s") and query.statuses.empty())
		return UsageError(ctx, self, "-s needs a status");
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
	std::string page = PageOf(e);

	ctx.out << Paint(DisplayName(e), output::kBold, ctx.color) << "\n";
	if (not e.english_title.empty())
		row("Romaji", e.title);
	row("Type", TypeToString(e.type));
	row("AniList", id_text(e.anilist_id));
	row("MyAnimeList", id_text(e.mal_id));
	if (not page.empty())
		row("Page", page);
	row("Status", Paint(StatusToString(e.status, e.type), StatusColor(e.status), ctx.color));
	row("Score",
	    e.score > 0
	        ? std::format("{}  {}/10", Paint(output::Stars(e.score), output::kYellow, ctx.color),
	                      e.score)
	        : "-");
	row("Progress", std::format("{} {}", OutOf(e.progress, e.total_units), UnitName(e.type)));
	if (e.type == MediaType::kManga)
		row("Volumes", OutOf(e.volumes, e.total_volumes));
	row(e.type == MediaType::kAnime ? "Rewatched" : "Reread", std::to_string(e.rewatched));
	row("Started", output::DateText(e.start_date));
	row("Finished", output::DateText(e.finish_date));
	row("Updated", output::DateText(e.updated_at));
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

	if (ctx.columns > 0) {
		for (size_t i = 0; i < found->size() and ctx.out; ++i) {
			ctx.out << (i > 0 ? "\n" : "");
			PrintCard(ctx, (*found)[i]);
		}
		return 0;
	}
	std::vector<std::vector<std::string>> rows;
	for (const auto& e : *found)
		rows.push_back(FullRowOf(e));
	WriteRows(ctx, rows);
	return 0;
}

// A list's numbers as a line: type, how many titles, how many in each status,
// the mean of the scores given, the episodes or chapters done (units), the
// volumes read, the rewatches, the days all of it took, and how many are
// being rewatched (a later field, so it comes last).
std::vector<std::string> StatsRow(const std::vector<Entry>& list, MediaType type) {
	std::vector<std::string> row = {TypeKey(type), std::to_string(list.size())};
	for (WatchStatus status : kStatuses)
		if (status != WatchStatus::kRewatching)
			row.push_back(std::to_string(CountByStatus(list, status)));
	double mean = AverageScore(list);
	row.push_back(mean > 0.0 ? std::format("{:.2f}", mean) : "0");
	row.push_back(std::to_string(TotalProgress(list)));
	row.push_back(std::to_string(TotalVolumes(list)));
	row.push_back(std::to_string(TotalRewatched(list)));
	row.push_back(std::format("{:.1f}", DaysSpent(list)));
	row.push_back(std::to_string(CountByStatus(list, WatchStatus::kRewatching)));
	return row;
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

	if (ctx.columns > 0) {
		for (size_t i = 0; i < types.size() and ctx.out; ++i) {
			ctx.out << (i > 0 ? "\n" : "");
			PrintStatsCard(ctx, ctx.db.GetEntries(types[i]), types[i]);
		}
		return 0;
	}
	std::vector<std::vector<std::string>> rows;
	for (MediaType type : types)
		rows.push_back(StatsRow(ctx.db.GetEntries(type), type));
	WriteRows(ctx, rows);
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
			    refs::TitleHolds(e, query.text))
				shown.push_back(e);
	}
	SortEntries(shown, query.key, query.reverse);

	if (query.refs_only) {
		for (const auto& e : shown)
			ctx.out << refs::Of(e) << "\n";
		return 0;
	}
	if (ctx.columns > 0) {
		if (shown.empty())
			SayNothingToList(ctx, query);
		else
			ShowTable(ctx, ListTable(ctx, shown));
		return 0;
	}
	std::vector<std::vector<std::string>> rows;
	for (const auto& e : shown)
		rows.push_back(RowOf(e));
	WriteRows(ctx, rows);
	return 0;
}

std::vector<Command> ListingCommands() {
	return {
	    {.name = "ls",
	     .summary = "List your titles",
	     .details =
	         std::string("Lists both lists in title order. QUERY keeps the titles whose\n"
	                     "English or romaji name holds all its words. -s takes several\n"
	                     "statuses with commas between them: -s watching,on-hold.\n\n") +
	         kRowFormat,
	     .options = {{'a', "", "", "list anime only"},
	                 {'m', "", "", "list manga only"},
	                 {'s', "", "STATUS",
	                  "only watching, completed, on-hold, dropped, planned, rewatching, or a,b"},
	                 {0, "sort", "KEY", "updated, score, progress, date, status or title"},
	                 {'r', "", "", "reverse the order"},
	                 {'q', "", "", "print the refs and nothing else"}},
	     .operands = "[QUERY...]",
	     .run = RunList},
	    {.name = "show",
	     .summary = "Show titles in full",
	     .details = "A TITLE is any part of its name. Into a pipe a title is one line: the\n"
	                "fields of ls, then total_volumes, anilist_id, mal_id, url (the title's\n"
	                "page on AniList) and romaji.\n",
	     .options = WithRefOptions({}),
	     .operands = "TITLE...",
	     .run = RunShow},
	    {.name = "stats",
	     .summary = "Show the numbers of your lists",
	     .details = "Into a pipe each list is one line of fields with a tab between them:\n"
	                "  type total watching completed on_hold dropped planned mean units\n"
	                "  volumes rewatched days rewatching\n"
	                "mean is the mean of the scores you gave (0 when there is none), units\n"
	                "the episodes or chapters done, and days an estimate of the time they\n"
	                "took: 24 minutes an episode, 5 minutes a chapter.\n",
	     .options = {{'a', "", "", "the anime list only"}, {'m', "", "", "the manga list only"}},
	     .run = RunStats},
	};
}

}  // namespace catalist::cli
