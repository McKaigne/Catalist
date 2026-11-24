#include <format>

#include "menu/common.hpp"
#include "menu/input.hpp"
#include "menu/output.hpp"

namespace catalist {

namespace {

void TakeStep(App& app, bool redoing) {
	auto taken = redoing ? app.db.Redo() : app.db.Undo();
	if (not taken.has_value())
		Fail(app, taken.error());
	else if (not taken.value())
		app.out << std::format("Nothing to {}.\n", redoing ? "redo" : "undo");
	else
		app.out << std::format("{} '{}'.\n", redoing ? "Redid" : "Undid", *taken.value());
}

void ShowHistory(App& app) {
	std::vector<std::vector<output::Styled>> rows;
	for (const auto& step : app.db.History())
		rows.push_back({{step.stamp},
		                {step.undone ? "undone" : "done", step.undone ? output::kDim : nullptr},
		                {step.label}});
	if (rows.empty())
		app.out << "Nothing has been changed since the program started.\n";
	else
		app.out << output::Rows({{""}, {""}, {"", false, true}}, rows, app.columns, app.color);
}

}  // namespace

void HistoryScreen(App& app) {
	int choice = 0;
	do {
		ClearScreen(app);
		DrawMenu(app, "UNDO AND HISTORY", {"Undo the last change", "Redo", "Show recent changes"});
		choice = ReadChoice(app, 3);
		switch (choice) {
			case 1:
				TakeStep(app, false);
				break;
			case 2:
				TakeStep(app, true);
				break;
			case 3:
				ShowHistory(app);
				break;
			default:
				break;
		}
		if (choice != 0)
			Pause(app);
	} while (choice != 0);
}

}  // namespace catalist
