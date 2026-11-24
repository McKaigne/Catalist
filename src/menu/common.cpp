#include "menu/common.hpp"

#include <algorithm>
#include <format>

#include "core/lists.hpp"
#include "core/text.hpp"
#include "menu/find.hpp"
#include "menu/input.hpp"
#include "menu/output.hpp"

namespace catalist {

namespace {

struct StatusChoice {
	const char* label;
	WatchStatus status;
};

constexpr StatusChoice kStatusChoices[kStatusCount] = {
    {"Planned", WatchStatus::kPlanToWatch},
    {"Watching or reading", WatchStatus::kWatching},
    {"Completed", WatchStatus::kCompleted},
    {"On hold", WatchStatus::kOnHold},
    {"Dropped", WatchStatus::kDropped},
    {"Rewatching or rereading", WatchStatus::kRewatching},
};

struct FieldChange {
	std::string field;
	std::string before;
	std::string after;
};

std::string DateOrDash(const std::optional<IsoDate>& day) {
	return day ? day->ToString() : "-";
}

std::vector<FieldChange> ChangedFields(const Entry& was, const Entry& now) {
	const FieldChange all[] = {
	    {"progress", std::to_string(was.progress), OutOf(now.progress, now.total_units)},
	    {"status", StatusKey(was.status), StatusKey(now.status)},
	    {"score", std::to_string(was.score), std::to_string(now.score)},
	    {"total", std::to_string(was.total_units), std::to_string(now.total_units)},
	    {"volumes", std::to_string(was.volumes), std::to_string(now.volumes)},
	    {"rewatched", std::to_string(was.rewatched), std::to_string(now.rewatched)},
	    {"started", DateOrDash(was.start_date), DateOrDash(now.start_date)},
	    {"finished", DateOrDash(was.finish_date), DateOrDash(now.finish_date)},
	    {"title", DisplayName(was), DisplayName(now)},
	};
	std::vector<FieldChange> changed;
	for (const auto& each : all) {
		bool same = each.field == "progress" ? was.progress == now.progress
		                                     : each.before == each.after;
		if (not same)
			changed.push_back(each);
	}
	return changed;
}

}  // namespace

void Fail(App& app, const std::string& message) {
	app.out << "Error: " << message << "\n";
}

void ClearScreen(App& app) {
	if (app.clear_screen)
		app.out << "\x1b[H\x1b[2J";
}

void DrawHeading(App& app, const std::string& heading) {
	app.out << output::Paint(heading, output::kBold, app.color) << "\n\n";
}

void DrawMenu(App& app, const std::string& title, const std::vector<std::string>& items,
              const std::string& leave) {
	DrawHeading(app, title);
	for (size_t i = 0; i < items.size(); ++i)
		app.out << std::format("  {:>2}  {}\n", i + 1, items[i]);
	app.out << std::format("  {:>2}  {}\n\n", 0, leave);
}

const char* StatusColor(WatchStatus status) {
	switch (status) {
		case WatchStatus::kWatching:
			return "32";
		case WatchStatus::kCompleted:
			return "35";
		case WatchStatus::kOnHold:
			return "33";
		case WatchStatus::kDropped:
			return "31";
		case WatchStatus::kPlanToWatch:
			return "90";
		case WatchStatus::kRewatching:
			return "36";
	}
	return nullptr;
}

std::string UnitName(MediaType type, int count) {
	if (type == MediaType::kAnime)
		return count == 1 ? "episode" : "episodes";
	return count == 1 ? "chapter" : "chapters";
}

std::string OutOf(int done, int total) {
	return std::format("{}/{}", done, total > 0 ? std::to_string(total) : std::string("?"));
}

std::vector<Entry> InProgress(const Catalogue& db) {
	std::vector<Entry> open;
	for (MediaType type : {MediaType::kAnime, MediaType::kManga})
		for (const auto& each : db.GetEntries(type))
			if (each.status == WatchStatus::kWatching or each.status == WatchStatus::kRewatching)
				open.push_back(each);
	SortEntries(open, SortKey::kUpdated, false);
	return open;
}

std::optional<WatchStatus> AskStatus(App& app, const std::string& title, int count) {
	std::vector<std::string> labels;
	for (int i = 0; i < count; ++i)
		labels.emplace_back(kStatusChoices[i].label);
	DrawMenu(app, title, labels);
	int choice = ReadChoice(app, count);
	if (choice == 0)
		return std::nullopt;
	return kStatusChoices[choice - 1].status;
}

std::optional<Entry> ChooseFrom(App& app, std::vector<Entry> titles) {
	constexpr size_t kMostOffered = 20;
	SortEntries(titles, SortKey::kUpdated, false);
	size_t more = titles.size() > kMostOffered ? titles.size() - kMostOffered : 0;
	titles.resize(titles.size() - more);
	std::vector<std::vector<output::Styled>> rows;
	for (size_t i = 0; i < titles.size(); ++i)
		rows.push_back({{std::format("  {:>2}", i + 1)},
		                {DisplayName(titles[i])},
		                {TypeKey(titles[i].type)},
		                {OutOf(titles[i].progress, titles[i].total_units)}});
	app.out << output::Rows({{""}, {"", false, true}, {""}, {""}}, rows, app.columns, app.color);
	if (more > 0)
		app.out << std::format("  and {} more: type more of the name to reach them\n", more);
	app.out << "   0  Back\n\n";
	int choice = ReadChoice(app, static_cast<int>(titles.size()));
	if (choice == 0)
		return std::nullopt;
	return titles[static_cast<size_t>(choice - 1)];
}

std::optional<Entry> ChooseTitle(App& app) {
	while (true) {
		auto text = AskText(app, "Part of the title (Enter to go back): ");
		if (not text)
			return std::nullopt;
		auto found = FindTitles(app.db, *text);
		if (found.size() == 1)
			return found[0];
		if (not found.empty()) {
			if (auto chosen = ChooseFrom(app, found))
				return chosen;
			continue;
		}
		auto closest = ClosestTitle(app.db, *text);
		if (closest and AskYes(app, std::format("Did you mean {}?", DisplayName(*closest))))
			return closest;
		if (not closest)
			app.out << std::format("Nothing in your lists matches '{}'.\n", *text);
	}
}

void ChangeTitle(App& app, const Entry& title, const std::string& action,
                 const std::function<Entry(const Entry&)>& change) {
	auto current = app.db.GetById(title.type, title.id);
	if (not current) {
		Fail(app, "that title is no longer in your lists");
		return;
	}
	Entry after = change(*current);
	auto fields = ChangedFields(*current, after);
	if (fields.empty()) {
		app.out << "Nothing changed.\n";
		return;
	}
	app.db.UpdateEntry(after);
	std::string label = utf8::Clip(std::format("{}: {}", action, DisplayName(after)), 60);
	if (not app.db.Save(label, app.Stamp())) {
		Fail(app, std::format("could not save {}", app.db.file().string()));
		return;
	}
	std::string name = output::Paint(DisplayName(after), output::kBold, app.color);
	for (const auto& each : fields)
		app.out << std::format("{}  {} {} -> {}\n", name, each.field, each.before, each.after);
}

}  // namespace catalist
