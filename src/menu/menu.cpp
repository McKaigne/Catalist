#include <algorithm>
#include <format>

#include "core/lists.hpp"
#include "menu/common.hpp"
#include "menu/input.hpp"
#include "menu/logo.hpp"
#include "menu/output.hpp"

namespace catalist {

namespace {

void DrawHome(App& app) {
	ClearScreen(app);
	DrawLogo(app, true);
	const auto& anime = app.db.GetEntries(MediaType::kAnime);
	const auto& manga = app.db.GetEntries(MediaType::kManga);
	size_t open = 0;
	for (const auto* list : {&anime, &manga})
		open += CountByStatus(*list, WatchStatus::kWatching) +
		        CountByStatus(*list, WatchStatus::kRewatching);
	auto total = [&app](const char* icon, size_t count, const char* label) {
		return std::format("{} {} {}", output::Paint(icon, output::kMagenta, app.color),
		                   output::Paint(std::to_string(count), output::kBold, app.color), label);
	};
	app.out << std::format("  {}    {}    {}\n", total(output::icon::kAnime, anime.size(), "anime"),
	                       total(output::icon::kManga, manga.size(), "manga"),
	                       total(output::icon::kWatching, open, "in progress"));
	app.out << output::Rule(std::min(app.columns, logo::kMiniWidth), app.color) << "\n\n";
	if (anime.empty() and manga.empty())
		app.out << std::format("{} Your lists are empty. Choose {} to add a title, or {} to import "
		                       "a list.\n\n",
		                       output::icon::kInfo, output::NumberHint(1), output::NumberHint(7));
}

}  // namespace

int RunMenu(App& app) {
	int choice = 0;
	do {
		DrawHome(app);
		DrawOptions(app,
		            {{output::icon::kAdd, "Add Title"},
		             {output::icon::kList, "View Lists"},
		             {output::icon::kLog, "Log Progress"},
		             {output::icon::kEdit, "Edit Title"},
		             {output::icon::kRemove, "Remove Title"},
		             {output::icon::kStats, "Stats"},
		             {output::icon::kTransfer, "Import & Export"}},
		            {output::icon::kExit, "Exit"});
		choice = ReadChoice(app, 7);
		switch (choice) {
			case 1:
				AddScreen(app);
				break;
			case 2:
				ViewScreen(app);
				break;
			case 3:
				LogProgressScreen(app);
				break;
			case 4:
				EditScreen(app);
				break;
			case 5:
				RemoveScreen(app);
				break;
			case 6:
				StatsScreen(app);
				break;
			case 7:
				TransferScreen(app);
				break;
			default:
				break;
		}
	} while (choice != 0);
	app.out << output::Paint("Goodbye!", output::kMagenta, app.color) << "\n";
	return 0;
}

}  // namespace catalist
