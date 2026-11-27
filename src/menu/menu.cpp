#include <algorithm>
#include <format>

#include "core/lists.hpp"
#include "menu/common.hpp"
#include "menu/input.hpp"
#include "menu/output.hpp"

namespace catalist {

namespace {

void DrawHome(App& app) {
	ClearScreen(app);
	DrawLogo(app);
	const auto& anime = app.db.GetEntries(MediaType::kAnime);
	const auto& manga = app.db.GetEntries(MediaType::kManga);
	size_t open = 0;
	for (const auto* list : {&anime, &manga})
		open += CountByStatus(*list, WatchStatus::kWatching) +
		        CountByStatus(*list, WatchStatus::kRewatching);
	auto total = [&app](size_t count, const char* label) {
		return std::format("{} {}", output::Paint(std::to_string(count), output::kBold, app.color),
		                   label);
	};
	app.out << std::format("  {}    {}    {}\n", total(anime.size(), "anime"),
	                       total(manga.size(), "manga"), total(open, "in progress"))
	        << output::Rule(std::min(app.columns, 80), app.color) << "\n\n";
	if (anime.empty() and manga.empty())
		Note(app,
		     "Your lists are empty. Open Anime or Manga and choose a to add a title, or "
		     "import a list.\n");
}

}  // namespace

int RunMenu(App& app) {
	int choice = 0;
	do {
		DrawHome(app);
		DrawOptions(app, {"Anime", "Manga", "Import & Export"}, "Exit");
		choice = ReadChoice(app, 3);
		switch (choice) {
			case 1:
				ListScreen(app, MediaType::kAnime);
				break;
			case 2:
				ListScreen(app, MediaType::kManga);
				break;
			case 3:
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
