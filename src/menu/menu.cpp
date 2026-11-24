#include <format>

#include "core/lists.hpp"
#include "menu/common.hpp"
#include "menu/input.hpp"
#include "menu/output.hpp"

namespace catalist {

namespace {

void DrawHome(App& app) {
	ClearScreen(app);
	app.out << output::Paint("Catalist", output::kBold, app.color) << "\n";
	const auto& anime = app.db.GetEntries(MediaType::kAnime);
	const auto& manga = app.db.GetEntries(MediaType::kManga);
	size_t open = 0;
	for (const auto* list : {&anime, &manga})
		open += CountByStatus(*list, WatchStatus::kWatching) +
		        CountByStatus(*list, WatchStatus::kRewatching);
	app.out << std::format("{} anime, {} manga, {} in progress\n\n", anime.size(), manga.size(),
	                       open);
	if (anime.empty() and manga.empty())
		app.out << "Your lists are empty. Choose 1 to add a title, or 7 to import a list.\n\n";
}

}  // namespace

int RunMenu(App& app) {
	int choice = 0;
	do {
		DrawHome(app);
		DrawOptions(app,
		            {"Add Title", "View Lists", "Log Progress", "Edit Title", "Remove Title", "Stats",
		             "Import & Export"},
		            "Exit");
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
	app.out << "Goodbye!\n";
	return 0;
}

}  // namespace catalist
