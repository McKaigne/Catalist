#include "menu/common.hpp"

#include <algorithm>
#include <format>

#include "core/lists.hpp"
#include "core/text.hpp"
#include "menu/find.hpp"
#include "menu/input.hpp"
#include "menu/logo.hpp"
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

std::vector<FieldChange> ChangedFields(const Entry& was, const Entry& now) {
	const FieldChange all[] = {
	    {"progress", std::to_string(was.progress), OutOf(now.progress, now.total_units)},
	    {"status", StatusKey(was.status), StatusKey(now.status)},
	    {"score", std::to_string(was.score), std::to_string(now.score)},
	    {"total", std::to_string(was.total_units), std::to_string(now.total_units)},
	    {"volumes", std::to_string(was.volumes), std::to_string(now.volumes)},
	    {"rewatched", std::to_string(was.rewatched), std::to_string(now.rewatched)},
	    {"rewatch value", RewatchValueLabel(was.rewatch_value),
	     RewatchValueLabel(now.rewatch_value)},
	    {"started", output::DateText(was.start_date), output::DateText(now.start_date)},
	    {"finished", output::DateText(was.finish_date), output::DateText(now.finish_date)},
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
	app.out << output::Paint(std::format("{} Error:", output::icon::kError), output::kRed, app.color)
	        << " " << message << "\n";
}

void Done(App& app, const std::string& message) {
	app.out << output::Paint(output::icon::kDone, output::kGreen, app.color) << " " << message
	        << "\n";
}

void Warn(App& app, const std::string& message) {
	app.out << output::Paint(output::icon::kError, output::kYellow, app.color) << " " << message
	        << "\n";
}

void ClearScreen(App& app) {
	if (app.clear_screen)
		app.out << "\x1b[H\x1b[2J";
}

void DrawLogo(App& app, bool big) {
	static const char* const kShades[] = {"38;5;117", "38;5;111", "38;5;141",
	                                      "38;5;177", "38;5;213", "38;5;219"};
	bool fits_big = big and app.columns >= logo::kBigWidth;
	if (not fits_big and app.columns < logo::kMiniWidth)
		return;
	std::string art = fits_big ? logo::kBig : logo::kMini;
	std::vector<std::string> lines;
	for (size_t start = 1, end; start < art.size(); start = end + 1) {
		end = art.find('\n', start);
		lines.push_back(art.substr(start, end - start));
	}
	for (size_t i = 0; i < lines.size(); ++i)
		app.out << output::Paint(lines[i], kShades[i * std::size(kShades) / lines.size()],
		                         app.color)
		        << "\n";
	app.out << "\n";
}

void DrawHeading(App& app, const std::string& heading) {
	DrawLogo(app, false);
	const std::string separator = " / ";
	std::string drawn = output::Paint(output::icon::kHome, output::kMagenta, app.color) + " ";
	size_t start = 0;
	while (true) {
		size_t end = heading.find(separator, start);
		bool last = end == std::string::npos;
		std::string part = heading.substr(start, last ? std::string::npos : end - start);
		drawn += output::Paint(part, last ? output::kBold : nullptr, app.color);
		if (last)
			break;
		drawn += output::Paint(separator, output::kGray, app.color);
		start = end + separator.size();
	}
	int width = std::min(app.columns, std::max(utf8::Width(heading) + 2, logo::kMiniWidth));
	app.out << drawn << "\n" << output::Rule(width, app.color) << "\n\n";
}

void DrawOption(App& app, int number, int digits, const Option& option) {
	app.out << std::format("  {}  {} {}\n",
	                       output::Paint(output::NumberHint(number, digits), output::kCyan,
	                                     app.color),
	                       output::Paint(option.icon, output::kMagenta, app.color), option.label);
}

void DrawOptions(App& app, const std::vector<Option>& items, const Option& leave) {
	int digits = static_cast<int>(std::to_string(items.size()).size());
	for (size_t i = 0; i < items.size(); ++i)
		DrawOption(app, static_cast<int>(i + 1), digits, items[i]);
	DrawOption(app, 0, digits, leave);
	app.out << "\n";
}

void DrawMenu(App& app, const std::string& heading, const std::vector<Option>& items) {
	DrawHeading(app, heading);
	DrawOptions(app, items);
}

const char* StatusColor(WatchStatus status) {
	switch (status) {
		case WatchStatus::kWatching:
			return output::kGreen;
		case WatchStatus::kCompleted:
			return output::kMagenta;
		case WatchStatus::kOnHold:
			return output::kYellow;
		case WatchStatus::kDropped:
			return output::kRed;
		case WatchStatus::kPlanToWatch:
			return output::kGray;
		case WatchStatus::kRewatching:
			return output::kCyan;
	}
	return nullptr;
}

const char* StatusIcon(WatchStatus status) {
	switch (status) {
		case WatchStatus::kWatching:
			return output::icon::kWatching;
		case WatchStatus::kCompleted:
			return output::icon::kCompleted;
		case WatchStatus::kOnHold:
			return output::icon::kOnHold;
		case WatchStatus::kDropped:
			return output::icon::kDropped;
		case WatchStatus::kPlanToWatch:
			return output::icon::kPlanned;
		case WatchStatus::kRewatching:
			return output::icon::kRewatching;
	}
	return output::icon::kInfo;
}

const char* TypeIcon(MediaType type) {
	return type == MediaType::kAnime ? output::icon::kAnime : output::icon::kManga;
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
	return open;
}

std::optional<WatchStatus> AskStatus(App& app, const std::string& heading, int count,
                                     std::optional<MediaType> type) {
	std::vector<Option> labels;
	for (int i = 0; i < count; ++i)
		labels.push_back({StatusIcon(kStatusOrder[i]), StatusLabel(kStatusOrder[i], type)});
	DrawMenu(app, heading, labels);
	int choice = ReadChoice(app, count);
	if (choice == 0)
		return std::nullopt;
	return kStatusOrder[choice - 1];
}

std::optional<Entry> ChooseFrom(App& app, std::vector<Entry> titles) {
	SortEntries(titles, SortKey::kUpdated);
	size_t pages = (titles.size() + kPageSize - 1) / kPageSize;
	size_t page = 0;
	while (true) {
		size_t first = page * kPageSize;
		size_t shown = std::min(kPageSize, titles.size() - first);
		int highest = static_cast<int>(shown) + (pages > 1 ? 2 : 0);
		int digits = static_cast<int>(std::to_string(highest).size());
		std::vector<std::vector<output::Styled>> rows;
		for (size_t i = 0; i < shown; ++i) {
			const Entry& e = titles[first + i];
			rows.push_back({{"  " + output::NumberHint(static_cast<int>(i + 1), digits),
			                 output::kCyan},
			                {TypeIcon(e.type), StatusColor(e.status)},
			                {DisplayName(e)},
			                {OutOf(e.progress, e.total_units)}});
		}
		app.out << output::Rows({{}, {}, {.flexible = true}, {.align_right = true}}, rows,
		                        app.columns, app.color);
		app.out << "\n";
		if (pages > 1) {
			app.out << output::Paint(std::format("  Page {} of {}", page + 1, pages),
			                         output::kGray, app.color)
			        << "\n";
			DrawOption(app, static_cast<int>(shown + 1), digits, {output::icon::kNext, "Next Page"});
			DrawOption(app, static_cast<int>(shown + 2), digits,
			           {output::icon::kPrevious, "Previous Page"});
		}
		DrawOption(app, 0, digits, kBackOption);
		app.out << "\n";

		int choice = ReadChoice(app, highest);
		if (choice == 0)
			return std::nullopt;
		auto picked = static_cast<size_t>(choice);
		if (picked <= shown)
			return titles[first + picked - 1];
		page = picked == shown + 1 ? std::min(page + 1, pages - 1) : (page > 0 ? page - 1 : 0);
		app.out << "\n";
	}
}

std::optional<Entry> ChooseTitle(App& app) {
	while (true) {
		auto text = AskText(app, std::format("{} Enter title (Enter to go back): ",
	                                     output::Paint(output::icon::kSearch, output::kMagenta, app.color)));
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
			app.out << std::format("{} Nothing in your lists matches '{}'.\n", output::icon::kInfo,
			                       *text);
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
		app.out << std::format("{} Nothing changed.\n", output::icon::kInfo);
		return;
	}
	app.db.UpdateEntry(after);
	if (not app.db.Save(app.Stamp())) {
		Fail(app, std::format("could not save {}", app.db.file().string()));
		return;
	}
	std::string name = output::Paint(DisplayName(after), output::kBold, app.color);
	std::string arrow = output::Paint(output::icon::kArrow, output::kCyan, app.color);
	for (const auto& each : fields)
		Done(app, std::format("{}  {} {} {} {}", name, each.field, each.before, arrow, each.after));
}

}  // namespace catalist
