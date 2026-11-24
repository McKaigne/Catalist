#include <algorithm>
#include <format>

#include "core/lists.hpp"
#include "menu/common.hpp"
#include "menu/find.hpp"
#include "menu/input.hpp"
#include "menu/output.hpp"

namespace catalist {

namespace {

constexpr size_t kPageSize = 20;

void PrintRows(App& app, const std::vector<Entry>& titles, size_t first, size_t last) {
	std::vector<output::Section> sections = {{"ANIME", {}}, {"MANGA", {}}};
	for (size_t i = first; i < last; ++i) {
		const Entry& e = titles[i];
		sections[e.type == MediaType::kAnime ? 0 : 1].rows.push_back(
		    {{DisplayName(e)},
		     {StatusToString(e.status, e.type), StatusColor(e.status)},
		     {output::Stars(e.score), output::kYellow},
		     {OutOf(e.progress, e.total_units)}});
	}
	app.out << output::Sections({{"", false, true}, {""}, {""}, {""}}, sections, app.columns,
	                            app.color);
}

void ShowPages(App& app, const std::string& heading, std::vector<Entry> titles, SortKey key) {
	if (titles.empty()) {
		app.out << "No titles to list.\n";
		Pause(app);
		return;
	}
	SortEntries(titles, key, false);
	std::stable_partition(titles.begin(), titles.end(),
	                      [](const Entry& e) { return e.type == MediaType::kAnime; });
	size_t pages = (titles.size() + kPageSize - 1) / kPageSize;
	size_t page = 0;
	while (true) {
		ClearScreen(app);
		DrawHeading(app, std::format("{} ({})", heading, titles.size()));
		PrintRows(app, titles, page * kPageSize, std::min(titles.size(), (page + 1) * kPageSize));
		if (pages == 1) {
			Pause(app);
			return;
		}
		app.out << std::format("\nPage {} of {}\n   1  Next page\n   2  Previous page\n   0  Back\n\n",
		                       page + 1, pages);
		switch (ReadChoice(app, 2)) {
			case 1:
				page = std::min(page + 1, pages - 1);
				break;
			case 2:
				page = page > 0 ? page - 1 : 0;
				break;
			default:
				return;
		}
	}
}

std::vector<Entry> TitlesWhere(const Catalogue& db, std::optional<MediaType> only,
                               std::optional<WatchStatus> status) {
	std::vector<Entry> kept;
	for (MediaType type : {MediaType::kAnime, MediaType::kManga}) {
		if (only and *only != type)
			continue;
		for (const auto& each : db.GetEntries(type))
			if (not status or each.status == *status)
				kept.push_back(each);
	}
	return kept;
}

void FindByName(App& app) {
	auto text = AskText(app, "Words in the title (Enter to go back): ");
	if (not text)
		return;
	std::vector<Entry> kept;
	for (const auto& each : TitlesWhere(app.db, std::nullopt, std::nullopt))
		if (TitleHolds(each, *text))
			kept.push_back(each);
	ShowPages(app, std::format("MATCHING '{}'", *text), kept, SortKey::kTitle);
}

void ShowDetails(App& app) {
	if (auto title = ChooseTitle(app)) {
		app.out << "\n";
		PrintCard(app, *title);
		Pause(app);
	}
}

void CardRow(App& app, const std::string& label, const std::string& value, int width = 13) {
	app.out << "  " << output::Paint(std::format("{:<{}}", label, width), output::kDim, app.color)
	        << value << "\n";
}

void PrintStats(App& app, MediaType type) {
	constexpr int kLabelWidth = 15;
	const auto& list = app.db.GetEntries(type);
	bool anime = type == MediaType::kAnime;
	double mean = AverageScore(list);
	auto row = [&app](const std::string& label, const std::string& value) {
		CardRow(app, label, value, kLabelWidth);
	};
	app.out << output::Paint(TypeToString(type), output::kBold, app.color) << "\n";
	row("Titles", std::to_string(list.size()));
	for (WatchStatus status : {WatchStatus::kWatching, WatchStatus::kCompleted, WatchStatus::kOnHold,
	                           WatchStatus::kDropped, WatchStatus::kPlanToWatch,
	                           WatchStatus::kRewatching})
		row(StatusToString(status, type), std::to_string(CountByStatus(list, status)));
	row("Mean score", mean > 0.0 ? std::format("{:.2f}", mean) : "-");
	row(anime ? "Episodes" : "Chapters", std::to_string(TotalProgress(list)));
	if (not anime)
		row("Volumes", std::to_string(TotalVolumes(list)));
	row(anime ? "Rewatched" : "Reread", std::to_string(TotalRewatched(list)));
	row("Days spent", std::format("{:.1f}", DaysSpent(list)));
}

}  // namespace

void PrintCard(App& app, const Entry& e) {
	auto row = [&app](const std::string& label, const std::string& value) {
		CardRow(app, label, value);
	};
	auto id_text = [](int id) { return id > 0 ? std::to_string(id) : std::string("-"); };
	auto date_text = [](const std::optional<IsoDate>& day) { return output::DateText(day); };
	std::string page = PageOf(e);

	app.out << output::Paint(DisplayName(e), output::kBold, app.color) << "\n";
	if (not e.english_title.empty())
		row("Romaji", e.title);
	row("Type", TypeToString(e.type));
	row("AniList", id_text(e.anilist_id));
	row("MyAnimeList", id_text(e.mal_id));
	if (not page.empty())
		row("Page", page);
	row("Status", output::Paint(StatusToString(e.status, e.type), StatusColor(e.status), app.color));
	row("Score", e.score > 0 ? std::format("{}  {}/10",
	                                       output::Paint(output::Stars(e.score), output::kYellow,
	                                                     app.color),
	                                       e.score)
	                         : "-");
	row("Progress", std::format("{} {}", OutOf(e.progress, e.total_units), UnitName(e.type)));
	if (e.type == MediaType::kManga)
		row("Volumes", OutOf(e.volumes, e.total_volumes));
	row(e.type == MediaType::kAnime ? "Rewatched" : "Reread", std::to_string(e.rewatched));
	row("Started", date_text(e.start_date));
	row("Finished", date_text(e.finish_date));
	row("Updated", date_text(e.updated_at));
}

void ViewScreen(App& app) {
	int choice = 0;
	do {
		ClearScreen(app);
		DrawMenu(app, "VIEW MY LISTS",
		         {"In progress", "All titles", "Anime only", "Manga only", "By status",
		          "Find by name", "Full details of a title"});
		choice = ReadChoice(app, 7);
		switch (choice) {
			case 1:
				ShowPages(app, "IN PROGRESS", InProgress(app.db), SortKey::kUpdated);
				break;
			case 2:
				ShowPages(app, "ALL TITLES", TitlesWhere(app.db, std::nullopt, std::nullopt),
				          SortKey::kTitle);
				break;
			case 3:
				ShowPages(app, "ANIME", TitlesWhere(app.db, MediaType::kAnime, std::nullopt),
				          SortKey::kTitle);
				break;
			case 4:
				ShowPages(app, "MANGA", TitlesWhere(app.db, MediaType::kManga, std::nullopt),
				          SortKey::kTitle);
				break;
			case 5:
				app.out << "\n";
				if (auto status = AskStatus(app, "Which status?", kStatusCount))
					ShowPages(app, StatusKey(*status), TitlesWhere(app.db, std::nullopt, status),
					          SortKey::kTitle);
				break;
			case 6:
				FindByName(app);
				break;
			case 7:
				ShowDetails(app);
				break;
			default:
				break;
		}
	} while (choice != 0);
}

void StatsScreen(App& app) {
	ClearScreen(app);
	DrawHeading(app, "STATISTICS");
	PrintStats(app, MediaType::kAnime);
	app.out << "\n";
	PrintStats(app, MediaType::kManga);
	Pause(app);
}

}  // namespace catalist
