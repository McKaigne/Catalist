#include <algorithm>
#include <format>

#include "menu/common.hpp"
#include "menu/input.hpp"
#include "menu/output.hpp"

namespace catalist {

namespace {

constexpr size_t kHomeRows = 5;

void DrawHome(App& app) {
	ClearScreen(app);
	app.out << output::Paint("Catalist", output::kBold, app.color) << "\n";
	size_t anime = app.db.GetEntries(MediaType::kAnime).size();
	size_t manga = app.db.GetEntries(MediaType::kManga).size();
	auto open = InProgress(app.db);
	app.out << std::format("{} anime, {} manga, {} in progress\n\n", anime, manga, open.size());
	if (anime + manga == 0)
		app.out << "Your lists are empty. Choose 1 to add a title, or 7 to import a list.\n\n";
	if (open.empty())
		return;
	open.resize(std::min(open.size(), kHomeRows));
	std::vector<std::vector<output::Styled>> rows;
	for (const auto& each : open)
		rows.push_back({{DisplayName(each)},
		                {TypeKey(each.type)},
		                {OutOf(each.progress, each.total_units)}});
	app.out << output::Sections({{"", false, true}, {""}, {""}}, {{"Continue", rows}}, app.columns,
	                            app.color)
	        << "\n";
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
