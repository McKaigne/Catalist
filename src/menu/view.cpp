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
const char* const kViewCrumb = "Catalist / View Lists";

void PrintRows(App& app, const std::vector<Entry>& titles, size_t first, size_t last) {
	std::vector<output::Section> sections = {{"Anime", {}}, {"Manga", {}}};
	for (size_t i = first; i < last; ++i) {
		const Entry& e = titles[i];
		sections[e.type == MediaType::kAnime ? 0 : 1].rows.push_back(
		    {{DisplayName(e)},
		     {StatusLabel(e.status, e.type), StatusColor(e.status)},
		     {output::Stars(e.score), output::kYellow},
		     {OutOf(e.progress, e.total_units)}});
	}
	app.out << output::Sections({{false, true}, {}, {}, {}}, sections, app.columns,
	                            app.color);
}

void ShowPages(App& app, const std::string& heading, std::vector<Entry> titles, SortKey key) {
	if (titles.empty()) {
		app.out << "No titles to list.\n";
		Pause(app);
		return;
	}
	SortEntries(titles, key);
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
		app.out << std::format("\nPage {} of {}\n", page + 1, pages);
		DrawOptions(app, {"Next Page", "Previous Page"});
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

void SearchLists(App& app) {
	ClearScreen(app);
	std::string crumb = std::format("{} / Search Lists", kViewCrumb);
	DrawHeading(app, crumb);
	auto text = AskText(app, "Enter title (Enter to go back): ");
	if (not text)
		return;
	std::vector<Entry> kept;
	for (const auto& each : TitlesWhere(app.db, std::nullopt, std::nullopt))
		if (TitleHolds(each, *text))
			kept.push_back(each);
	ShowPages(app, std::format("{} / {}", crumb, *text), kept, SortKey::kTitle);
}

void ShowDetails(App& app) {
	ClearScreen(app);
	std::string crumb = std::format("{} / Title Details", kViewCrumb);
	DrawHeading(app, crumb);
	if (auto title = ChooseTitle(app)) {
		ClearScreen(app);
		DrawHeading(app, std::format("{} / {}", crumb, DisplayName(*title)));
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
		row(StatusLabel(status, type), std::to_string(CountByStatus(list, status)));
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
	std::string page = PageOf(e);

	if (not e.english_title.empty())
		row("Romaji", e.title);
	row("Type", TypeToString(e.type));
	row("AniList", id_text(e.anilist_id));
	row("MyAnimeList", id_text(e.mal_id));
	if (not page.empty())
		row("Page", page);
	row("Status", output::Paint(StatusLabel(e.status, e.type), StatusColor(e.status), app.color));
	row("Score", e.score > 0 ? std::format("{}  {}/10",
	                                       output::Paint(output::Stars(e.score), output::kYellow,
	                                                     app.color),
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

void ViewScreen(App& app) {
	int choice = 0;
	do {
		ClearScreen(app);
		DrawMenu(app, kViewCrumb,
		         {"In Progress", "All Titles", "Anime", "Manga", "By Status", "Search Lists",
		          "Title Details"});
		choice = ReadChoice(app, 7);
		switch (choice) {
			case 1:
				ShowPages(app, std::format("{} / In Progress", kViewCrumb), InProgress(app.db),
				          SortKey::kUpdated);
				break;
			case 2:
				ShowPages(app, std::format("{} / All Titles", kViewCrumb),
				          TitlesWhere(app.db, std::nullopt, std::nullopt), SortKey::kTitle);
				break;
			case 3:
				ShowPages(app, std::format("{} / Anime", kViewCrumb),
				          TitlesWhere(app.db, MediaType::kAnime, std::nullopt), SortKey::kTitle);
				break;
			case 4:
				ShowPages(app, std::format("{} / Manga", kViewCrumb),
				          TitlesWhere(app.db, MediaType::kManga, std::nullopt), SortKey::kTitle);
				break;
			case 5:
				ClearScreen(app);
				if (auto status = AskStatus(app, std::format("{} / By Status", kViewCrumb),
				                            kStatusCount, std::nullopt))
					ShowPages(app,
					          std::format("{} / By Status / {}", kViewCrumb,
					                      StatusLabel(*status, std::nullopt)),
					          TitlesWhere(app.db, std::nullopt, status), SortKey::kTitle);
				break;
			case 6:
				SearchLists(app);
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
	DrawHeading(app, "Catalist / Stats");
	PrintStats(app, MediaType::kAnime);
	app.out << "\n";
	PrintStats(app, MediaType::kManga);
	Pause(app);
}

}  // namespace catalist
