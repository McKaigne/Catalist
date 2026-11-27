#include <format>
#include <functional>
#include <utility>

#include "menu/common.hpp"
#include "menu/input.hpp"
#include "menu/output.hpp"

namespace catalist {

namespace {

void PrintCard(App& app, const Entry& e) {
	auto id_text = [](int id) { return id > 0 ? std::to_string(id) : std::string("-"); };
	bool anime = e.type == MediaType::kAnime;
	std::string page = PageOf(e);

	if (not e.english_title.empty())
		CardRow(app, "Romaji", e.title);
	if (anime)
		CardRow(app, "Format", MediaFormatLabel(e.format));
	CardRow(app, "Status",
	        output::Paint(std::format("{} {}", StatusMark(e.status), StatusLabel(e.status, e.type)),
	                      StatusColor(e.status), app.color));
	CardRow(app, "Score",
	        e.score > 0
	            ? std::format("{}  {}/10",
				              output::Paint(output::Stars(e.score), output::kYellow, app.color),
				              e.score)
				: "-");
	std::string bar = output::Bar(e.progress, e.total_units, 20, app.color);
	CardRow(app, "Progress",
	        std::format("{}{}{} {}", bar, bar.empty() ? "" : "  ", OutOf(e.progress, e.total_units),
	                    UnitName(e.type)));
	if (not anime)
		CardRow(app, "Volumes", OutOf(e.volumes, e.total_volumes));
	CardRow(app, anime ? "Rewatched" : "Reread", std::to_string(e.rewatched));
	CardRow(app, "Started", output::DateText(e.start_date));
	CardRow(app, "Finished", output::DateText(e.finish_date));
	CardRow(app, "Updated", output::DateText(e.updated_at));
	CardRow(app, "AniList", id_text(e.anilist_id));
	CardRow(app, "MyAnimeList", id_text(e.mal_id));
	if (not page.empty())
		CardRow(app, "Page", output::Paint(page, output::kCyan, app.color));
}

void CountOne(App& app, const Entry& title) {
	if (AtTotal(title)) {
		Note(app, std::format("{} is at its total, {} {}. If there are more, set the total in "
		                      "Edit Details.",
		                      DisplayName(title), title.total_units,
		                      UnitName(title.type, title.total_units)));
		return;
	}
	IsoDate today = app.Today();
	ChangeTitle(app, title, [&](const Entry& e) { return StepProgress(e, 1, today); });
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

void AskNewStatus(App& app, const Entry& title, const std::string& crumb) {
	ClearScreen(app);
	auto status = AskStatus(app, crumb + " / Set Status", kStatusCount, title.type);
	if (not status)
		return;
	if (auto reason = StatusRefusal(title, *status)) {
		Fail(app, *reason);
		return;
	}
	IsoDate today = app.Today();
	ChangeTitle(app, title, [&](const Entry& e) { return SetStatus(e, *status, today); });
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

void EditDetails(App& app, const Entry& title, const std::string& crumb) {
	using Action = std::function<void(const Entry&)>;
	std::vector<std::pair<std::string, Action>> actions = {
	    {"Set Total",
		 [&](const Entry& e) { AskCount(app, e, "New total", CountField::kTotalUnits); }}};
	if (title.type == MediaType::kManga)
		actions.push_back({"Set Volumes Read", [&](const Entry& e) {
			                   AskCount(app, e, "Volumes read", CountField::kVolumes);
		                   }});
	actions.push_back({"Set Start Date", [&](const Entry& e) { AskDate(app, e, true); }});
	actions.push_back({"Set Finish Date", [&](const Entry& e) { AskDate(app, e, false); }});
	actions.push_back({"Rename", [&](const Entry& e) { AskName(app, e); }});
	std::vector<std::string> labels;
	for (const auto& each : actions)
		labels.push_back(each.first);

	while (true) {
		auto current = app.db.GetById(title.type, title.id);
		if (not current)
			return;
		ClearScreen(app);
		DrawMenu(app, crumb + " / Edit Details", labels);
		int choice = ReadChoice(app, static_cast<int>(labels.size()));
		if (choice == 0)
			return;
		actions[static_cast<size_t>(choice - 1)].second(*current);
		Pause(app);
	}
}

bool Remove(App& app, const Entry& title) {
	if (not AskYes(app, std::format("Remove {} from your {} list?", DisplayName(title),
	                                TypeKey(title.type)))) {
		Note(app, "Nothing was removed.");
		return false;
	}
	app.db.RemoveEntry(title.type, title.id);
	if (not app.db.Save(app.Stamp())) {
		Fail(app, std::format("could not save {}", app.db.file().string()));
		return false;
	}
	Done(app, std::format("{} removed.", DisplayName(title)));
	return true;
}

}  // namespace

void TitleScreen(App& app, const Entry& title) {
	std::string one = title.type == MediaType::kAnime ? "+1 Episode" : "+1 Chapter";
	while (true) {
		auto current = app.db.GetById(title.type, title.id);
		if (not current)
			return;
		std::string crumb =
		    std::format("Catalist / {} / {}", TypeToString(title.type), DisplayName(*current));
		ClearScreen(app);
		DrawHeading(app, crumb);
		PrintCard(app, *current);
		app.out << "\n";
		DrawOptions(app,
		            {one, "Set Progress", "Set Status", "Set Score", "Edit Details", "Remove"});
		switch (ReadChoice(app, 6)) {
			case 1:
				CountOne(app, *current);
				break;
			case 2:
				AskCount(app, *current, "New progress", CountField::kProgress);
				break;
			case 3:
				AskNewStatus(app, *current, crumb);
				break;
			case 4:
				AskScore(app, *current);
				break;
			case 5:
				EditDetails(app, *current, crumb);
				continue;
			case 6:
				if (Remove(app, *current)) {
					Pause(app);
					return;
				}
				break;
			default:
				return;
		}
		Pause(app);
	}
}

}  // namespace catalist
