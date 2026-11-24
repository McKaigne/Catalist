#include <format>

#include "menu/common.hpp"
#include "menu/input.hpp"

namespace catalist {

namespace {

void CountBy(App& app, const Entry& title, int delta) {
	auto current = app.db.GetById(title.type, title.id);
	if (current and delta > 0 and AtTotal(*current)) {
		app.out << std::format("{} is at its total, {} {}. If there are more, use Set total.\n",
		                       DisplayName(*current), current->total_units,
		                       UnitName(current->type, current->total_units));
		return;
	}
	IsoDate today = app.Today();
	ChangeTitle(app, title, delta > 0 ? "Count one more" : "Count one less",
	            [&](const Entry& e) { return StepProgress(e, delta, today); });
}

void AskCount(App& app, const Entry& title, const std::string& action, CountField field) {
	auto text = AskText(app, std::format("{} (Enter to go back): ", action));
	if (not text)
		return;
	auto number = ParseNumber(*text);
	if (not number) {
		app.out << "Enter a whole number.\n";
		return;
	}
	IsoDate today = app.Today();
	ChangeTitle(app, title, action,
	            [&](const Entry& e) { return SetCount(e, field, *number, today); });
}

void AskScore(App& app, const Entry& title) {
	auto text = AskText(app, "Score from 0 to 10 (Enter to go back): ");
	if (not text)
		return;
	auto number = ParseNumber(*text);
	if (not number or *number > 10) {
		app.out << "Enter a number from 0 to 10.\n";
		return;
	}
	IsoDate today = app.Today();
	ChangeTitle(app, title, "Set score",
	            [&](const Entry& e) { return SetScore(e, *number, today); });
}

void AskNewStatus(App& app, const Entry& title) {
	app.out << "\n";
	auto status = AskStatus(app, "New status", kStatusCount);
	if (not status)
		return;
	auto current = app.db.GetById(title.type, title.id);
	if (not current)
		return;
	if (auto reason = StatusRefusal(*current, *status)) {
		Fail(app, *reason);
		return;
	}
	IsoDate today = app.Today();
	ChangeTitle(app, title, "Change status",
	            [&](const Entry& e) { return SetStatus(e, *status, today); });
}

void AskDate(App& app, const Entry& title, bool start) {
	auto text = AskText(app, "Date as YYYY-MM-DD, today, or none (Enter to go back): ");
	if (not text)
		return;
	IsoDate today = app.Today();
	std::optional<IsoDate> day;
	if (*text == "today")
		day = today;
	else if (*text != "none")
		day = IsoDate::Parse(*text);
	if (*text != "none" and not day) {
		app.out << "Enter a real date such as 2026-10-20.\n";
		return;
	}
	ChangeTitle(app, title, start ? "Set start date" : "Set finish date", [&](const Entry& e) {
		return start ? SetStartDate(e, day, today) : SetFinishDate(e, day, today);
	});
}

void AskName(App& app, const Entry& title) {
	auto name = AskText(app, "New name (Enter to go back): ");
	if (not name)
		return;
	IsoDate today = app.Today();
	ChangeTitle(app, title, "Rename", [&](const Entry& e) { return SetTitle(e, *name, today); });
}

}  // namespace

void CountNextScreen(App& app) {
	ClearScreen(app);
	DrawHeading(app, "COUNT THE NEXT EPISODE OR CHAPTER");
	auto open = InProgress(app.db);
	if (open.empty()) {
		app.out << "Nothing is in progress.\n";
		Pause(app);
		return;
	}
	auto title = ChooseFrom(app, open);
	if (not title)
		return;
	CountBy(app, *title, 1);
	Pause(app);
}

void EditScreen(App& app) {
	ClearScreen(app);
	DrawHeading(app, "EDIT A TITLE");
	auto chosen = ChooseTitle(app);
	if (not chosen)
		return;
	int choice = 0;
	do {
		auto current = app.db.GetById(chosen->type, chosen->id);
		if (not current)
			return;
		ClearScreen(app);
		PrintCard(app, *current);
		app.out << "\n";
		DrawMenu(app, "EDIT",
		         {"Count one more", "Count one less", "Set progress", "Set score", "Change status",
		          "Set total", "Set volumes (manga)", "Set start date", "Set finish date",
		          "Rename"});
		choice = ReadChoice(app, 10);
		switch (choice) {
			case 1:
				CountBy(app, *current, 1);
				break;
			case 2:
				CountBy(app, *current, -1);
				break;
			case 3:
				AskCount(app, *current, "Set progress", CountField::kProgress);
				break;
			case 4:
				AskScore(app, *current);
				break;
			case 5:
				AskNewStatus(app, *current);
				break;
			case 6:
				AskCount(app, *current, "Set total", CountField::kTotalUnits);
				break;
			case 7:
				if (current->type == MediaType::kManga)
					AskCount(app, *current, "Set volumes", CountField::kVolumes);
				else
					app.out << "Only manga have volumes.\n";
				break;
			case 8:
				AskDate(app, *current, true);
				break;
			case 9:
				AskDate(app, *current, false);
				break;
			case 10:
				AskName(app, *current);
				break;
			default:
				break;
		}
		if (choice != 0)
			Pause(app);
	} while (choice != 0);
}

void RemoveScreen(App& app) {
	ClearScreen(app);
	DrawHeading(app, "REMOVE A TITLE");
	auto title = ChooseTitle(app);
	if (not title)
		return;
	std::string name = std::format("{} ({})", DisplayName(*title), TypeKey(title->type));
	if (not AskYes(app, std::format("Remove {} from your lists?", name))) {
		app.out << "Cancelled.\n";
		Pause(app);
		return;
	}
	app.db.RemoveEntry(title->type, title->id);
	if (app.db.Save(std::format("Remove: {}", DisplayName(*title)), app.Stamp()))
		app.out << std::format("{} removed.\n", name);
	else
		Fail(app, std::format("could not save {}", app.db.file().string()));
	Pause(app);
}

}  // namespace catalist
