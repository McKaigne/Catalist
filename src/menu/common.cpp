#include "menu/common.hpp"

#include <algorithm>
#include <format>

#include "core/text.hpp"
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
	    {"started", output::DateText(was.start_date), output::DateText(now.start_date)},
	    {"finished", output::DateText(was.finish_date), output::DateText(now.finish_date)},
	    {"title", DisplayName(was), DisplayName(now)},
	};
	std::vector<FieldChange> changed;
	for (const auto& each : all) {
		bool same =
		    each.field == "progress" ? was.progress == now.progress : each.before == each.after;
		if (not same)
			changed.push_back(each);
	}
	return changed;
}

}  // namespace

void Fail(App& app, const std::string& message) {
	app.out << output::Paint("Error:", output::kRed, app.color) << " " << message << "\n";
}

void Done(App& app, const std::string& message) {
	app.out << output::Paint("OK", output::kGreen, app.color) << " " << message << "\n";
}

void Warn(App& app, const std::string& message) {
	app.out << output::Paint("!", output::kYellow, app.color) << " " << message << "\n";
}

void Note(App& app, const std::string& message) {
	app.out << output::Paint("-", output::kCyan, app.color) << " " << message << "\n";
}

void ClearScreen(App& app) {
	if (app.clear_screen)
		app.out << "\x1b[H\x1b[2J";
}

void DrawLogo(App& app) {
	static const char* const kShades[] = {"38;5;117", "38;5;111", "38;5;141",
	                                      "38;5;177", "38;5;213", "38;5;219"};
	bool big = app.columns >= logo::kBigWidth;
	if (not big and app.columns < logo::kMiniWidth)
		return;
	std::string art = big ? logo::kBig : logo::kMini;
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
	const std::string separator = " / ";
	std::string drawn;
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
	int width = std::min(app.columns, std::max(utf8::Width(heading), logo::kMiniWidth));
	app.out << drawn << "\n" << output::Rule(width, app.color) << "\n\n";
}

void DrawOption(App& app, int number, int digits, const std::string& label) {
	app.out << std::format(
	    "  {}  {}\n",
	    output::Paint(std::format("{:>{}}", number, digits), output::kCyan, app.color), label);
}

void DrawOptions(App& app, const std::vector<std::string>& items, const std::string& leave) {
	int digits = static_cast<int>(std::to_string(items.size()).size());
	for (size_t i = 0; i < items.size(); ++i)
		DrawOption(app, static_cast<int>(i + 1), digits, items[i]);
	DrawOption(app, 0, digits, leave);
	app.out << "\n";
}

void DrawMenu(App& app, const std::string& heading, const std::vector<std::string>& items) {
	DrawHeading(app, heading);
	DrawOptions(app, items);
}

void CardRow(App& app, const std::string& label, const std::string& value, int width) {
	app.out << "  " << output::Paint(std::format("{:<{}}", label, width), output::kDim, app.color)
	        << value << "\n";
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

std::string StatusMark(WatchStatus status) {
	switch (status) {
		case WatchStatus::kWatching:
			return "[W]";
		case WatchStatus::kCompleted:
			return "[C]";
		case WatchStatus::kOnHold:
			return "[H]";
		case WatchStatus::kDropped:
			return "[D]";
		case WatchStatus::kPlanToWatch:
			return "[P]";
		case WatchStatus::kRewatching:
			return "[R]";
	}
	return "[?]";
}

std::string StatusLabel(WatchStatus status, MediaType type) {
	bool manga = type == MediaType::kManga;
	switch (status) {
		case WatchStatus::kPlanToWatch:
			return manga ? "Plan to Read" : "Plan to Watch";
		case WatchStatus::kWatching:
			return manga ? "Reading" : "Watching";
		case WatchStatus::kCompleted:
			return "Completed";
		case WatchStatus::kOnHold:
			return "On Hold";
		case WatchStatus::kDropped:
			return "Dropped";
		case WatchStatus::kRewatching:
			return manga ? "Rereading" : "Rewatching";
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

std::optional<WatchStatus> AskStatus(App& app, const std::string& heading, int count,
                                     MediaType type) {
	std::vector<std::string> labels;
	for (int i = 0; i < count; ++i)
		labels.push_back(
		    std::format("{} {}", StatusMark(kStatusOrder[i]), StatusLabel(kStatusOrder[i], type)));
	DrawMenu(app, heading, labels);
	int choice = ReadChoice(app, count);
	if (choice == 0)
		return std::nullopt;
	return kStatusOrder[choice - 1];
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
		Note(app, "Nothing changed.");
		return;
	}
	app.db.UpdateEntry(after);
	if (not app.db.Save(app.Stamp())) {
		Fail(app, std::format("could not save {}", app.db.file().string()));
		return;
	}
	std::string name = output::Paint(DisplayName(after), output::kBold, app.color);
	std::string arrow = output::Paint("->", output::kCyan, app.color);
	for (const auto& each : fields)
		Done(app, std::format("{}  {} {} {} {}", name, each.field, each.before, arrow, each.after));
}

}  // namespace catalist
