#include <format>
#include <iterator>

#include "menu/common.hpp"
#include "menu/input.hpp"
#include "menu/output.hpp"

namespace catalist {

namespace {

std::string RewatchValueName(MediaType type) {
	return type == MediaType::kAnime ? "Set Rewatch Value" : "Set Reread Value";
}

void CountBy(App& app, const Entry& title, int delta) {
	auto current = app.db.GetById(title.type, title.id);
	if (current and delta > 0 and AtTotal(*current)) {
		app.out << std::format("{} {} is at its total, {} {}. If there are more, use Set Total.\n",
		                       output::icon::kInfo, DisplayName(*current), current->total_units,
		                       UnitName(current->type, current->total_units));
		return;
	}
	IsoDate today = app.Today();
	ChangeTitle(app, title, [&](const Entry& e) { return StepProgress(e, delta, today); });
}

void AskCount(App& app, const Entry& title, const std::string& prompt, CountField field) {
	auto text = AskText(app, std::format("{} (Enter to go back): ", prompt));
	if (not text)
		return;
	auto number = ParseNumber(*text);
	if (not number) {
		Warn(app, "Enter a whole number.");
		return;
	}
	IsoDate today = app.Today();
	ChangeTitle(app, title, [&](const Entry& e) { return SetCount(e, field, *number, today); });
}

void AskScore(App& app, const Entry& title) {
	auto text = AskText(app, "Score from 0 to 10 (Enter to go back): ");
	if (not text)
		return;
	auto number = ParseNumber(*text);
	if (not number or *number > 10) {
		Warn(app, "Enter a number from 0 to 10.");
		return;
	}
	IsoDate today = app.Today();
	ChangeTitle(app, title, [&](const Entry& e) { return SetScore(e, *number, today); });
}

void AskNewStatus(App& app, const Entry& title) {
	ClearScreen(app);
	auto status = AskStatus(app,
	                        std::format("Catalist / Edit Title / {} / Set Status", DisplayName(title)),
	                        kStatusCount, title.type);
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
	ChangeTitle(app, title, [&](const Entry& e) { return SetStatus(e, *status, today); });
}

void AskRewatchValue(App& app, const Entry& title) {
	constexpr RewatchValue kValues[] = {RewatchValue::kVeryLow, RewatchValue::kLow,
	                                    RewatchValue::kMedium, RewatchValue::kHigh,
	                                    RewatchValue::kVeryHigh, RewatchValue::kNone};
	ClearScreen(app);
	std::vector<Option> labels;
	for (RewatchValue value : kValues)
		labels.push_back({output::icon::kRewatch,
		                  value == RewatchValue::kNone ? "None" : RewatchValueLabel(value)});
	DrawMenu(app, std::format("Catalist / Edit Title / {} / {}", DisplayName(title),
	                          RewatchValueName(title.type)),
	         labels);
	int choice = ReadChoice(app, static_cast<int>(std::size(kValues)));
	if (choice == 0)
		return;
	IsoDate today = app.Today();
	ChangeTitle(app, title, [&](const Entry& e) {
		return SetRewatchValue(e, kValues[choice - 1], today);
	});
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
		Warn(app, "Enter a real date such as 2026-10-20.");
		return;
	}
	ChangeTitle(app, title, [&](const Entry& e) {
		return start ? SetStartDate(e, day, today) : SetFinishDate(e, day, today);
	});
}

void AskName(App& app, const Entry& title) {
	auto name = AskText(app, "New name (Enter to go back): ");
	if (not name)
		return;
	IsoDate today = app.Today();
	ChangeTitle(app, title, [&](const Entry& e) { return SetTitle(e, *name, today); });
}

}  // namespace

void LogProgressScreen(App& app) {
	ClearScreen(app);
	DrawHeading(app, "Catalist / Log Progress");
	auto open = InProgress(app.db);
	if (open.empty()) {
		app.out << std::format("{} Nothing is in progress.\n", output::icon::kInfo);
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
	DrawHeading(app, "Catalist / Edit Title");
	auto chosen = ChooseTitle(app);
	if (not chosen)
		return;
	int choice = 0;
	do {
		auto current = app.db.GetById(chosen->type, chosen->id);
		if (not current)
			return;
		ClearScreen(app);
		DrawHeading(app, std::format("Catalist / Edit Title / {}", DisplayName(*current)));
		PrintCard(app, *current);
		app.out << "\n";
		namespace icon = output::icon;
		DrawOptions(app, {{icon::kPlus, "+1 Progress"},
		                  {icon::kMinus, "-1 Progress"},
		                  {icon::kProgress, "Set Progress"},
		                  {icon::kScore, "Set Score"},
		                  {icon::kStatus, "Set Status"},
		                  {icon::kTotal, "Set Total"},
		                  {icon::kManga, "Set Volumes"},
		                  {icon::kStart, "Set Start Date"},
		                  {icon::kFinish, "Set Finish Date"},
		                  {icon::kRename, "Rename Title"},
		                  {icon::kRewatch, RewatchValueName(current->type)}});
		choice = ReadChoice(app, 11);
		switch (choice) {
			case 1:
				CountBy(app, *current, 1);
				break;
			case 2:
				CountBy(app, *current, -1);
				break;
			case 3:
				AskCount(app, *current, "New progress", CountField::kProgress);
				break;
			case 4:
				AskScore(app, *current);
				break;
			case 5:
				AskNewStatus(app, *current);
				break;
			case 6:
				AskCount(app, *current, "New total", CountField::kTotalUnits);
				break;
			case 7:
				if (current->type == MediaType::kManga)
					AskCount(app, *current, "Volumes read", CountField::kVolumes);
				else
					app.out << std::format("{} Only manga have volumes.\n", output::icon::kInfo);
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
			case 11:
				AskRewatchValue(app, *current);
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
	DrawHeading(app, "Catalist / Remove Title");
	auto title = ChooseTitle(app);
	if (not title)
		return;
	std::string name = std::format("{} ({})", DisplayName(*title), TypeKey(title->type));
	if (not AskYes(app, std::format("Remove {} from your lists?", name))) {
		app.out << std::format("{} Cancelled.\n", output::icon::kInfo);
		Pause(app);
		return;
	}
	app.db.RemoveEntry(title->type, title->id);
	if (app.db.Save(app.Stamp()))
		Done(app, std::format("{} removed.", name));
	else
		Fail(app, std::format("could not save {}", app.db.file().string()));
	Pause(app);
}

}  // namespace catalist
