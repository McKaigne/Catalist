#include <algorithm>
#include <filesystem>
#include <format>
#include <set>
#include <string_view>

#include "menu/common.hpp"
#include "menu/input.hpp"
#include "storage/mal_xml.hpp"
#include "system.hpp"

namespace catalist {

namespace {

struct Clash {
	std::string field;
	std::string yours;
	std::string file;
};

// Where both hold a value and the values differ. A value only one of them has
// is no clash: an import fills in what is empty.
std::vector<Clash> ClashesOf(const Entry& mine, const Entry& file) {
	std::vector<Clash> clashes;
	int total = file.total_units > 0 ? file.total_units : mine.total_units;
	if (mine.status != file.status)
		clashes.push_back({"status", StatusKey(mine.status), StatusKey(file.status)});
	auto count = [&](const char* field, int Entry::* value, bool out_of_total = false) {
		if (mine.*value == 0 or file.*value == 0 or mine.*value == file.*value)
			return;
		clashes.push_back({field,
		                   out_of_total ? OutOf(mine.*value, total) : std::to_string(mine.*value),
		                   out_of_total ? OutOf(file.*value, total) : std::to_string(file.*value)});
	};
	auto date = [&](const char* field, std::optional<IsoDate> Entry::* value) {
		if (mine.*value and file.*value and mine.*value != file.*value)
			clashes.push_back({field, FormatDate(mine.*value), FormatDate(file.*value)});
	};
	count("score", &Entry::score);
	count("progress", &Entry::progress, true);
	count("volumes", &Entry::volumes);
	count("rewatched", &Entry::rewatched);
	date("started", &Entry::start_date);
	date("finished", &Entry::finish_date);
	return clashes;
}

void PrintClashes(App& app, const Entry& mine, const std::vector<Clash>& clashes) {
	size_t width = 0;
	for (const auto& each : clashes)
		width = std::max(width, each.yours.size());
	app.out << std::format("{} ({})\n", DisplayName(mine), TypeKey(mine.type));
	for (const auto& each : clashes)
		app.out << std::format("  {:<9} yours {:<{}}   file {}\n", each.field, each.yours, width,
		                       each.file);
	app.out << "\n";
}

char AskClash(App& app) {
	while (true) {
		auto answer = ReadLine(app, "[k]eep  [f]ile  [K]eep rest  [F]ile rest  [q]uit: ");
		if (not answer)
			return 'q';
		if (answer->size() == 1 and std::string_view("kfKFq").contains((*answer)[0]))
			return (*answer)[0];
	}
}

std::optional<std::set<const Entry*>> AskClashes(App& app, const std::vector<Entry>& titles) {
	std::set<const Entry*> take_file;
	std::optional<char> rest;
	for (const auto& title : titles) {
		auto mine = app.db.FindImported(title);
		auto clashes = mine ? ClashesOf(*mine, title) : std::vector<Clash>{};
		if (clashes.empty())
			continue;
		char answer = rest.value_or('\0');
		if (not rest) {
			PrintClashes(app, *mine, clashes);
			answer = AskClash(app);
		}
		if (answer == 'q')
			return std::nullopt;
		if (answer == 'K' or answer == 'F')
			rest = answer;
		if (answer == 'f' or answer == 'F')
			take_file.insert(&title);
	}
	return take_file;
}

void ImportFile(App& app) {
	auto typed = AskText(app, "MyAnimeList XML file to import (Enter to go back): ");
	if (not typed)
		return;
	std::string path = ExpandHome(*typed);
	auto content = ReadFile(path);
	if (not content) {
		Fail(app, std::format("cannot read {}", path));
		return;
	}
	if (content->starts_with("\x1f\x8b")) {
		Fail(app, std::format("{} is compressed; unpack it first with gunzip", path));
		return;
	}
	auto titles = mal_xml::Parse(*content);
	if (titles.empty()) {
		Fail(app, std::format("no anime or manga found in {}", path));
		return;
	}
	auto take_file = AskClashes(app, titles);
	if (not take_file) {
		app.out << "Cancelled. Nothing was imported.\n";
		return;
	}

	size_t added = 0;
	for (const auto& title : titles) {
		size_t before = app.db.GetEntries(title.type).size();
		int id = app.db.ImportEntry(title);
		added += app.db.GetEntries(title.type).size() - before;
		if (take_file->contains(&title))
			app.db.UpdateEntry(TakeImported(*app.db.GetById(title.type, id), title));
	}
	if (not app.db.Save(std::format("Import: {}", std::filesystem::path(path).filename().string()),
	                    app.Stamp())) {
		Fail(app, std::format("could not save {}", app.db.file().string()));
		return;
	}
	app.out << std::format("{}: {} titles, {} added, {} in your lists already.\n", path,
	                       titles.size(), added, titles.size() - added);
}

void ExportList(App& app, MediaType type) {
	auto typed = AskText(app, std::format("File to save the {} list in (Enter to go back): ",
	                                      TypeKey(type)));
	if (not typed)
		return;
	std::string path = ExpandHome(*typed);
	std::error_code ignored;
	if (std::filesystem::exists(path, ignored) and
	    not AskYes(app, std::format("{} exists. Replace it?", path))) {
		app.out << "Nothing was saved.\n";
		return;
	}
	auto written = mal_xml::Write(app.db.GetEntries(type), type);
	if (not WriteFile(path, written.xml)) {
		Fail(app, std::format("could not write {}", path));
		return;
	}
	app.out << std::format("Saved {}.\n", path);
	if (written.skipped > 0)
		app.out << std::format("{} without a MyAnimeList id left out.\n", written.skipped);
}

}  // namespace

void TransferScreen(App& app) {
	int choice = 0;
	do {
		ClearScreen(app);
		DrawMenu(app, "IMPORT OR EXPORT",
		         {"Import a MyAnimeList XML file", "Export anime", "Export manga"});
		choice = ReadChoice(app, 3);
		switch (choice) {
			case 1:
				ImportFile(app);
				break;
			case 2:
				ExportList(app, MediaType::kAnime);
				break;
			case 3:
				ExportList(app, MediaType::kManga);
				break;
			default:
				break;
		}
		if (choice != 0)
			Pause(app);
	} while (choice != 0);
}

}  // namespace catalist
