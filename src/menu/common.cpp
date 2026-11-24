#include "menu/common.hpp"

#include <algorithm>
#include <format>

#include "core/lists.hpp"
#include "menu/find.hpp"
#include "menu/input.hpp"
#include "menu/output.hpp"

namespace catalist {

namespace {

constexpr WatchStatus kStatusOrder[kStatusCount] = {
    WatchStatus::kPlanToWatch, WatchStatus::kWatching, WatchStatus::kCompleted,
    WatchStatus::kOnHold,      WatchStatus::kDropped,  WatchStatus::kRewatching,
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

void DrawOptions(App& app, const std::vector<std::string>& items, const std::string& leave) {
	size_t width = std::to_string(items.size()).size();
	for (size_t i = 0; i < items.size(); ++i)
		app.out << std::format("{:>{}}. {}\n", i + 1, width, items[i]);
	app.out << std::format("{:>{}}. {}\n\n", 0, width, leave);
}

void DrawMenu(App& app, const std::string& heading, const std::vector<std::string>& items) {
	DrawHeading(app, heading);
	DrawOptions(app, items);
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

std::string StatusLabel(WatchStatus status, std::optional<MediaType> type) {
	bool manga = type == MediaType::kManga;
	switch (status) {
		case WatchStatus::kPlanToWatch:
			return not type ? "Planned" : manga ? "Plan to Read" : "Plan to Watch";
		case WatchStatus::kWatching:
			return not type ? "Watching/Reading" : manga ? "Reading" : "Watching";
		case WatchStatus::kCompleted:
			return "Completed";
		case WatchStatus::kOnHold:
			return "On Hold";
		case WatchStatus::kDropped:
			return "Dropped";
		case WatchStatus::kRewatching:
			return not type ? "Rewatching/Rereading" : manga ? "Rereading" : "Rewatching";
	}
	return "";
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

std::optional<WatchStatus> AskStatus(App& app, const std::string& heading, int count,
                                     std::optional<MediaType> type) {
	std::vector<std::string> labels;
	for (int i = 0; i < count; ++i)
		labels.push_back(StatusLabel(kStatusOrder[i], type));
	DrawMenu(app, heading, labels);
	int choice = ReadChoice(app, count);
	if (choice == 0)
		return std::nullopt;
	return kStatusOrder[choice - 1];
}

std::optional<Entry> ChooseFrom(App& app, std::vector<Entry> titles) {
	constexpr size_t kMostOffered = 20;
	SortEntries(titles, SortKey::kUpdated, false);
	size_t more = titles.size() > kMostOffered ? titles.size() - kMostOffered : 0;
	titles.resize(titles.size() - more);
	std::vector<std::vector<output::Styled>> rows;
	for (size_t i = 0; i < titles.size(); ++i)
		rows.push_back({{std::format("{}.", i + 1)},
		                {DisplayName(titles[i])},
		                {TypeKey(titles[i].type)},
		                {OutOf(titles[i].progress, titles[i].total_units)}});
	app.out << output::Rows({{"", true}, {"", false, true}, {""}, {""}}, rows, app.columns,
	                        app.color);
	if (more > 0)
		app.out << std::format("and {} more: type more of the name to reach them\n", more);
	app.out << std::format("{:>{}}. Back\n\n", 0, std::to_string(titles.size()).size());
	int choice = ReadChoice(app, static_cast<int>(titles.size()));
	if (choice == 0)
		return std::nullopt;
	return titles[static_cast<size_t>(choice - 1)];
}

std::optional<Entry> ChooseTitle(App& app) {
	while (true) {
		auto text = AskText(app, "Enter title (Enter to go back): ");
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

void ChangeTitle(App& app, const Entry& title, const std::function<Entry(const Entry&)>& change) {
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
	if (not app.db.Save(app.Stamp())) {
		Fail(app, std::format("could not save {}", app.db.file().string()));
		return;
	}
	std::string name = output::Paint(DisplayName(after), output::kBold, app.color);
	for (const auto& each : fields)
		app.out << std::format("{}  {} {} -> {}\n", name, each.field, each.before, each.after);
}

}  // namespace catalist
