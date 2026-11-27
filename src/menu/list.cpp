#include <algorithm>
#include <cctype>
#include <format>

#include "core/lists.hpp"
#include "menu/common.hpp"
#include "menu/find.hpp"
#include "menu/input.hpp"
#include "menu/output.hpp"

namespace catalist {

namespace {

struct SortChoice {
	SortKey key;
	const char* label;
};

constexpr SortChoice kSorts[] = {
    {SortKey::kUpdated, "Last Updated"}, {SortKey::kTitle, "Title"},
    {SortKey::kStatus, "Status"},        {SortKey::kScore, "Score"},
    {SortKey::kProgress, "Progress"},    {SortKey::kStartDate, "Start Date"},
};

constexpr MediaFormat kFormats[] = {
    MediaFormat::kTv,      MediaFormat::kTvShort, MediaFormat::kTvSpecial, MediaFormat::kMovie,
    MediaFormat::kSpecial, MediaFormat::kOva,     MediaFormat::kOna,       MediaFormat::kMusic,
};

struct ListView {
	MediaType type = MediaType::kAnime;
	std::string search;
	std::optional<WatchStatus> status;
	// 0 is no score.
	std::optional<int> score;
	std::optional<MediaFormat> format;
	SortKey sort = SortKey::kUpdated;
	bool reversed = false;
	size_t page = 0;
};

bool Shows(const ListView& view, const Entry& e) {
	return (not view.status or e.status == *view.status) and
	       (not view.score or e.score == *view.score) and
	       (not view.format or e.format == *view.format) and
	       (view.search.empty() or TitleHolds(e, view.search));
}

std::vector<Entry> Shown(const App& app, const ListView& view) {
	std::vector<Entry> shown;
	for (const auto& each : app.db.GetEntries(view.type))
		if (Shows(view, each))
			shown.push_back(each);
	SortEntries(shown, view.sort);
	if (view.reversed)
		std::ranges::reverse(shown);
	return shown;
}

std::string SortLabel(SortKey key) {
	for (const auto& each : kSorts)
		if (each.key == key)
			return each.label;
	return "";
}

// What the list is narrowed to, and how it is sorted.
std::string Narrowed(const ListView& view) {
	std::string text;
	auto add = [&text](const std::string& part) { text += (text.empty() ? "" : ", ") + part; };
	if (not view.search.empty())
		add(std::format("'{}'", view.search));
	if (view.status)
		add(StatusLabel(*view.status, view.type));
	if (view.score)
		add(*view.score == 0 ? "no score" : std::format("score {}", *view.score));
	if (view.format)
		add(MediaFormatLabel(*view.format));
	return text;
}

void PrintRows(App& app, const ListView& view, const std::vector<Entry>& shown, size_t first,
               size_t last) {
	bool anime = view.type == MediaType::kAnime;
	int digits = static_cast<int>(std::to_string(last - first).size());
	std::vector<std::vector<output::Styled>> rows;
	for (size_t i = first; i < last; ++i) {
		const Entry& e = shown[i];
		std::vector<output::Styled> row = {
		    {std::format("  {:>{}}", i - first + 1, digits), output::kCyan},
		    {StatusMark(e.status), StatusColor(e.status)},
		    {DisplayName(e)}};
		if (anime)
			row.push_back({MediaFormatLabel(e.format)});
		row.push_back({output::Stars(e.score), output::kYellow});
		row.push_back({OutOf(e.progress, e.total_units), output::kCyan});
		rows.push_back(row);
	}
	std::vector<output::TableColumn> columns = {{}, {}, {.flexible = true}};
	if (anime)
		columns.push_back({});
	columns.push_back({});
	columns.push_back({.align_right = true});
	app.out << output::Rows(columns, rows, app.columns, app.color);
}

void PrintKeys(App& app, bool paged) {
	auto key = [&app](const char* letter, const char* label) {
		return std::format("{} {}", output::Paint(letter, output::kCyan, app.color), label);
	};
	app.out << "\n  " << key("#", "Open") << "   " << key("f", "Filter") << "   "
	        << key("s", "Sort") << "   " << key("c", "Clear") << "   " << key("a", "Add") << "   "
	        << key("t", "Stats");
	if (paged)
		app.out << "   " << key("n", "Next") << "   " << key("p", "Previous");
	app.out << "   " << key("0", "Back") << "\n\n";
}

void PrintStats(App& app, MediaType type) {
	constexpr int kLabelWidth = 22;
	constexpr int kBarWidth = 20;
	const auto& list = app.db.GetEntries(type);
	bool anime = type == MediaType::kAnime;
	double mean = AverageScore(list);
	int count = static_cast<int>(list.size());
	auto row = [&app](const std::string& label, const std::string& value) {
		CardRow(app, label, value, kLabelWidth);
	};
	row("Titles", std::to_string(count));
	for (WatchStatus status :
	     {WatchStatus::kWatching, WatchStatus::kCompleted, WatchStatus::kOnHold,
	      WatchStatus::kDropped, WatchStatus::kPlanToWatch, WatchStatus::kRewatching}) {
		int some = static_cast<int>(CountByStatus(list, status));
		row(std::format("{} {}", StatusMark(status), StatusLabel(status, type)),
		    std::format("{:<7}{}", some, output::Bar(some, count, kBarWidth, app.color)));
	}
	row("Mean score",
	    mean > 0.0 ? std::format("{:.2f}  {}", mean,
	                             output::Paint(output::Stars(mean), output::kYellow, app.color))
	               : "-");
	row(anime ? "Episodes" : "Chapters", std::to_string(TotalProgress(list)));
	if (not anime)
		row("Volumes", std::to_string(TotalVolumes(list)));
	row(anime ? "Rewatched" : "Reread", std::to_string(TotalRewatched(list)));
	row("Days spent", std::format("{:.1f}", DaysSpent(list)));
}

void AskScoreFilter(App& app, ListView& view) {
	auto text = AskText(app, "Score from 1 to 10, or 0 for no score (Enter to go back): ");
	if (not text)
		return;
	auto number = ParseNumber(*text);
	if (not number or *number > 10) {
		Warn(app, "Enter a number from 0 to 10.");
		Pause(app);
		return;
	}
	view.score = *number;
}

void AskFormatFilter(App& app, ListView& view, const std::string& heading) {
	ClearScreen(app);
	std::vector<std::string> labels;
	for (MediaFormat format : kFormats)
		labels.push_back(MediaFormatLabel(format));
	DrawMenu(app, heading, labels);
	int choice = ReadChoice(app, static_cast<int>(labels.size()));
	if (choice > 0)
		view.format = kFormats[choice - 1];
}

void ClearFilters(ListView& view) {
	view.search.clear();
	view.status.reset();
	view.score.reset();
	view.format.reset();
}

void AskFilter(App& app, ListView& view, const std::string& crumb) {
	bool anime = view.type == MediaType::kAnime;
	std::string heading = crumb + " / Filter";
	ClearScreen(app);
	std::vector<std::string> options = {"By Status", "By Score"};
	if (anime)
		options.push_back("By Format");
	options.push_back("Clear Filters");
	DrawMenu(app, heading, options);
	int choice = ReadChoice(app, static_cast<int>(options.size()));
	if (not anime and choice == 3)
		choice = 4;
	switch (choice) {
		case 1:
			ClearScreen(app);
			if (auto status = AskStatus(app, heading + " / By Status", kStatusCount, view.type))
				view.status = status;
			break;
		case 2:
			AskScoreFilter(app, view);
			break;
		case 3:
			AskFormatFilter(app, view, heading + " / By Format");
			break;
		case 4:
			ClearFilters(view);
			break;
		default:
			return;
	}
	view.page = 0;
}

void AskSort(App& app, ListView& view, const std::string& crumb) {
	ClearScreen(app);
	std::vector<std::string> options;
	for (const auto& each : kSorts)
		options.push_back(each.label);
	options.push_back("Reverse Order");
	DrawMenu(app, crumb + " / Sort", options);
	int choice = ReadChoice(app, static_cast<int>(options.size()));
	if (choice == 0)
		return;
	if (choice == static_cast<int>(options.size())) {
		view.reversed = not view.reversed;
	} else {
		view.sort = kSorts[choice - 1].key;
		view.reversed = false;
	}
	view.page = 0;
}

void ShowStats(App& app, MediaType type, const std::string& crumb) {
	ClearScreen(app);
	DrawHeading(app, crumb + " / Stats");
	PrintStats(app, type);
	Pause(app);
}

}  // namespace

void ListScreen(App& app, MediaType type) {
	ListView view;
	view.type = type;
	std::string crumb = std::format("Catalist / {}", TypeToString(type));
	std::string notice;
	while (true) {
		auto shown = Shown(app, view);
		size_t total = app.db.GetEntries(type).size();
		size_t pages = std::max<size_t>(1, (shown.size() + kPageSize - 1) / kPageSize);
		view.page = std::min(view.page, pages - 1);
		size_t first = view.page * kPageSize;
		size_t last = std::min(shown.size(), first + kPageSize);

		ClearScreen(app);
		DrawHeading(app, crumb);
		std::string narrowed = Narrowed(view);
		app.out << output::Paint(
		               std::format("  {} of {} titles{}   Sorted by {}{}", shown.size(), total,
					               narrowed.empty() ? "" : ": " + narrowed, SortLabel(view.sort),
					               view.reversed ? ", reversed" : ""),
		               output::kGray, app.color)
		        << "\n\n";
		if (shown.empty()) {
			if (total == 0)
				Note(app,
				     std::format("Your {} list is empty. Choose a to add a title.", TypeKey(type)));
			else
				Note(app, "No titles match. Choose c to clear the search and filters.");
		} else {
			PrintRows(app, view, shown, first, last);
		}
		if (pages > 1)
			app.out << output::Paint(std::format("\n  Page {} of {}", view.page + 1, pages),
			                         output::kGray, app.color)
			        << "\n";
		PrintKeys(app, pages > 1);
		if (not notice.empty()) {
			Warn(app, notice);
			notice.clear();
		}

		auto answer = ReadLine(app, std::format("{} Number, letter, or text to search: ",
		                                        output::Paint(">", output::kMagenta, app.color)));
		if (not answer)
			return;
		if (answer->empty())
			continue;
		if (auto number = ParseNumber(*answer)) {
			if (*number == 0)
				return;
			if (static_cast<size_t>(*number) <= last - first)
				TitleScreen(app, shown[first + static_cast<size_t>(*number) - 1]);
			else
				notice = std::format("There is no title {} on this page.", *number);
			continue;
		}
		char letter =
		    answer->size() == 1
		        ? static_cast<char>(std::tolower(static_cast<unsigned char>((*answer)[0])))
		        : '\0';
		switch (letter) {
			case 'f':
				AskFilter(app, view, crumb);
				break;
			case 's':
				AskSort(app, view, crumb);
				break;
			case 'c':
				ClearFilters(view);
				view.page = 0;
				break;
			case 'a':
				AddScreen(app, type);
				break;
			case 't':
				ShowStats(app, type, crumb);
				break;
			case 'n':
				view.page = std::min(view.page + 1, pages - 1);
				break;
			case 'p':
				view.page = view.page > 0 ? view.page - 1 : 0;
				break;
			default: {
				ListView searched = view;
				searched.search = *answer;
				searched.page = 0;
				if (Shown(app, searched).empty()) {
					auto closest = ClosestTitle(app.db.GetEntries(type), *answer);
					notice = closest ? std::format("Nothing matches '{}'. Did you mean {}?",
					                               *answer, DisplayName(*closest))
					                 : std::format("Nothing matches '{}'.", *answer);
				} else {
					view = searched;
				}
				break;
			}
		}
	}
}

}  // namespace catalist
