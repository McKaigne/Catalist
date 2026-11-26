#include <algorithm>
#include <format>

#include "core/lists.hpp"
#include "menu/common.hpp"
#include "menu/find.hpp"
#include "menu/input.hpp"
#include "menu/output.hpp"

namespace catalist {

namespace {

const char* const kViewCrumb = "Catalist / View Lists";

void PrintRows(App& app, const std::vector<Entry>& titles, size_t first, size_t last) {
	std::vector<output::Section> sections = {
	    {std::format("{} Anime", output::icon::kAnime), {}},
	    {std::format("{} Manga", output::icon::kManga), {}}};
	for (size_t i = first; i < last; ++i) {
		const Entry& e = titles[i];
		sections[e.type == MediaType::kAnime ? 0 : 1].rows.push_back(
		    {{"  " + std::string(StatusIcon(e.status)), StatusColor(e.status)},
		     {DisplayName(e)},
		     {StatusLabel(e.status, e.type), StatusColor(e.status)},
		     {output::Stars(e.score), output::kYellow},
		     {OutOf(e.progress, e.total_units), output::kCyan}});
	}
	app.out << output::Sections({{}, {.flexible = true}, {}, {}, {.align_right = true}}, sections,
	                            app.columns, app.color);
}

void ShowPages(App& app, const std::string& heading, std::vector<Entry> titles, SortKey key) {
	if (titles.empty()) {
		app.out << std::format("{} No titles to list.\n", output::icon::kInfo);
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
		app.out << output::Paint(std::format("\n  Page {} of {}", page + 1, pages), output::kGray,
		                         app.color)
		        << "\n";
		DrawOptions(app, {{output::icon::kNext, "Next Page"},
		                  {output::icon::kPrevious, "Previous Page"}});
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
	auto text = AskText(app, std::format("{} Enter title (Enter to go back): ",
	                                     output::Paint(output::icon::kSearch, output::kMagenta, app.color)));
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

void CardRow(App& app, const char* icon, const std::string& label, const std::string& value,
             int width = 14) {
	app.out << "  " << output::Paint(icon, output::kMagenta, app.color) << " "
	        << output::Paint(std::format("{:<{}}", label, width), output::kDim, app.color) << value
	        << "\n";
}

void PrintStats(App& app, MediaType type) {
	constexpr int kLabelWidth = 22;
	constexpr int kBarWidth = 20;
	const auto& list = app.db.GetEntries(type);
	bool anime = type == MediaType::kAnime;
	double mean = AverageScore(list);
	int count = static_cast<int>(list.size());
	auto row = [&app](const char* icon, const std::string& label, const std::string& value) {
		CardRow(app, icon, label, value, kLabelWidth);
	};
	app.out << output::Paint(std::format("{} {}", TypeIcon(type), TypeToString(type)),
	                         output::kBold, app.color)
	        << "\n";
	row(output::icon::kList, "Titles", std::to_string(count));
	for (WatchStatus status : {WatchStatus::kWatching, WatchStatus::kCompleted, WatchStatus::kOnHold,
	                           WatchStatus::kDropped, WatchStatus::kPlanToWatch,
	                           WatchStatus::kRewatching}) {
		int some = static_cast<int>(CountByStatus(list, status));
		row(StatusIcon(status), StatusLabel(status, type),
		    std::format("{:<7}{}", some, output::Bar(some, count, kBarWidth, app.color)));
	}
	row(output::icon::kScore, "Mean score",
	    mean > 0.0 ? std::format("{:.2f}  {}", mean,
	                             output::Paint(output::Stars(mean), output::kYellow, app.color))
	               : "-");
	row(output::icon::kProgress, anime ? "Episodes" : "Chapters",
	    std::to_string(TotalProgress(list)));
	if (not anime)
		row(output::icon::kManga, "Volumes", std::to_string(TotalVolumes(list)));
	row(output::icon::kRewatch, anime ? "Rewatched" : "Reread",
	    std::to_string(TotalRewatched(list)));
	row(output::icon::kPlanned, "Days spent", std::format("{:.1f}", DaysSpent(list)));
}

}  // namespace

void PrintCard(App& app, const Entry& e) {
	namespace icon = output::icon;
	auto row = [&app](const char* icon, const std::string& label, const std::string& value) {
		CardRow(app, icon, label, value);
	};
	auto id_text = [](int id) { return id > 0 ? std::to_string(id) : std::string("-"); };
	bool anime = e.type == MediaType::kAnime;
	std::string page = PageOf(e);

	if (not e.english_title.empty())
		row(icon::kRename, "Romaji", e.title);
	row(TypeIcon(e.type), "Type", TypeToString(e.type));
	row(icon::kInfo, "AniList", id_text(e.anilist_id));
	row(icon::kInfo, "MyAnimeList", id_text(e.mal_id));
	if (not page.empty())
		row(icon::kSearch, "Page", output::Paint(page, output::kCyan, app.color));
	row(StatusIcon(e.status), "Status",
	    output::Paint(StatusLabel(e.status, e.type), StatusColor(e.status), app.color));
	row(icon::kScore, "Score",
	    e.score > 0 ? std::format("{}  {}/10",
	                              output::Paint(output::Stars(e.score), output::kYellow, app.color),
	                              e.score)
	                : "-");
	std::string bar = output::Bar(e.progress, e.total_units, 20, app.color);
	row(icon::kProgress, "Progress",
	    std::format("{}{}{} {}", bar, bar.empty() ? "" : "  ", OutOf(e.progress, e.total_units),
	                UnitName(e.type)));
	if (not anime)
		row(icon::kManga, "Volumes", OutOf(e.volumes, e.total_volumes));
	row(icon::kTotal, anime ? "Rewatched" : "Reread", std::to_string(e.rewatched));
	row(icon::kRewatch, anime ? "Rewatch value" : "Reread value",
	    RewatchValueLabel(e.rewatch_value));
	row(icon::kStart, "Started", output::DateText(e.start_date));
	row(icon::kFinish, "Finished", output::DateText(e.finish_date));
	row(icon::kPlanned, "Updated", output::DateText(e.updated_at));
}

void ViewScreen(App& app) {
	int choice = 0;
	do {
		ClearScreen(app);
		DrawMenu(app, kViewCrumb,
		         {{output::icon::kWatching, "In Progress"},
		          {output::icon::kList, "All Titles"},
		          {output::icon::kAnime, "Anime"},
		          {output::icon::kManga, "Manga"},
		          {output::icon::kFilter, "By Status"},
		          {output::icon::kSearch, "Search Lists"},
		          {output::icon::kInfo, "Title Details"}});
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
