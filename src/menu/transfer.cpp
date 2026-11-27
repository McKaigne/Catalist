#include <algorithm>
#include <filesystem>
#include <format>
#include <set>
#include <string_view>
#include <utility>

#include "core/text.hpp"
#include "menu/common.hpp"
#include "menu/input.hpp"
#include "menu/output.hpp"
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
	size_t field_width = 9;
	for (const auto& each : clashes) {
		width = std::max(width, each.yours.size());
		field_width = std::max(field_width, each.field.size() + 1);
	}
	app.out << std::format("{} ({})\n", output::Paint(DisplayName(mine), output::kBold, app.color),
	                       TypeKey(mine.type));
	for (const auto& each : clashes)
		app.out << std::format("  {:<{}} yours {:<{}}   file {}\n", each.field, field_width,
		                       each.yours, width, each.file);
	app.out << "\n";
}

// k keeps yours, f takes the file's, r and t do the same for every title left,
// q stops. K and F still mean r and t.
char AskClash(App& app) {
	std::string keys;
	for (auto [key, label] : {std::pair{'k', "Keep"},
	                          {'f', "File"},
	                          {'r', "Keep Rest"},
	                          {'t', "File Rest"},
	                          {'q', "Quit"}})
		keys += std::format("{} {}  ", output::Paint(std::string(1, key), output::kCyan, app.color),
		                    label);
	while (true) {
		auto answer = ReadLine(app, keys.substr(0, keys.size() - 2) + ": ");
		if (not answer)
			return 'q';
		if (*answer == "K" or *answer == "F")
			return *answer == "K" ? 'r' : 't';
		std::string word = utf8::Lowercase(*answer);
		if (word.size() == 1 and std::string_view("kfrtq").contains(word[0]))
			return word[0];
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
		if (answer == 'r' or answer == 't')
			rest = answer;
		if (answer == 'f' or answer == 't')
			take_file.insert(&title);
	}
	return take_file;
}

// The file name MyAnimeList gives an export, as an example.
std::string ExampleFile(MediaType type) {
	return std::format("{}list.xml", TypeKey(type));
}

// How a typed path is read, with examples that would all work here.
void ShowPathHelp(App& app, MediaType type) {
	std::error_code ignored;
	std::string name = ExampleFile(type);
	auto gray = [&app](const std::string& text) {
		return output::Paint(text, output::kGray, app.color);
	};
	app.out << std::format("  A name alone means this folder: {}\n",
	                       gray(std::filesystem::current_path(ignored).string()))
	        << std::format("  ~ means your home folder: {}\n\n", gray(ExpandHome("~")))
	        << "  For example:\n";
	for (const auto& path : {name, "~/Downloads/" + name, ExpandHome("~/Downloads/" + name)})
		app.out << "    " << output::Paint(path, output::kCyan, app.color) << "\n";
	app.out << "\n";
}

std::string FullPath(const std::string& typed) {
	std::error_code ignored;
	auto full = std::filesystem::absolute(ExpandHome(typed), ignored);
	return full.empty() ? typed : full.lexically_normal().string();
}

// False when the user went back without choosing a file.
bool ImportFile(App& app, MediaType type, const std::string& heading) {
	ClearScreen(app);
	DrawHeading(app, heading);
	app.out << std::format(
	    "  Choose the {} list you exported from MyAnimeList, an .xml file.\n"
	    "  If it ends in .xml.gz, unpack it first: gunzip {}.gz\n\n",
	    TypeKey(type), ExampleFile(type));
	ShowPathHelp(app, type);
	auto typed = AskText(app, std::format("{} File to import (Enter to go back): ",
	                                      output::Paint(">", output::kMagenta, app.color)));
	if (not typed)
		return false;
	std::string path = FullPath(*typed);
	auto content = ReadFile(path);
	if (not content) {
		Fail(app, std::format("cannot read {}", path));
		return true;
	}
	if (content->starts_with("\x1f\x8b")) {
		Fail(app, std::format("{} is compressed; unpack it first with gunzip", path));
		return true;
	}
	auto titles = mal_xml::Parse(*content);
	auto others = std::erase_if(titles, [type](const Entry& e) { return e.type != type; });
	if (titles.empty()) {
		MediaType other = type == MediaType::kAnime ? MediaType::kManga : MediaType::kAnime;
		Fail(app, others > 0 ? std::format("{} holds {}, not {}; choose Import {} XML for it", path,
		                                   TypeKey(other), TypeKey(type), TypeToString(other))
		                     : std::format("no {} found in {}", TypeKey(type), path));
		return true;
	}
	auto take_file = AskClashes(app, titles);
	if (not take_file) {
		Note(app, "Cancelled. Nothing was imported.");
		return true;
	}

	size_t added = 0;
	for (const auto& title : titles) {
		size_t before = app.db.GetEntries(title.type).size();
		int id = app.db.ImportEntry(title);
		added += app.db.GetEntries(title.type).size() - before;
		if (take_file->contains(&title))
			app.db.UpdateEntry(TakeImported(*app.db.GetById(title.type, id), title));
	}
	if (not app.db.Save(app.Stamp())) {
		Fail(app, std::format("could not save {}", app.db.file().string()));
		return true;
	}
	Done(app, std::format("{}: {} titles, {} added, {} in your {} list already.", path,
	                      titles.size(), added, titles.size() - added, TypeKey(type)));
	return true;
}

bool ExportList(App& app, MediaType type, const std::string& heading) {
	ClearScreen(app);
	DrawHeading(app, heading);
	app.out << std::format(
	    "  Saves your {} list as MyAnimeList XML, which AniList and MyAnimeList import.\n\n",
	    TypeKey(type));
	ShowPathHelp(app, type);
	auto typed = AskText(app, std::format("{} File to save (Enter to go back): ",
	                                      output::Paint(">", output::kMagenta, app.color)));
	if (not typed)
		return false;
	std::string path = FullPath(*typed);
	std::error_code ignored;
	if (std::filesystem::exists(path, ignored) and
	    not AskYes(app, std::format("{} exists. Replace it?", path))) {
		Note(app, "Nothing was saved.");
		return true;
	}
	auto written = mal_xml::Write(app.db.GetEntries(type), type);
	if (not WriteFile(path, written.xml)) {
		Fail(app, std::format("could not write {}", path));
		return true;
	}
	Done(app, std::format("Saved {}.", path));
	if (written.skipped > 0)
		Note(app, std::format("{} without a MyAnimeList id left out.", written.skipped));
	return true;
}

}  // namespace

void TransferScreen(App& app) {
	const std::string crumb = "Catalist / Import & Export";
	const std::vector<std::string> options = {"Import Anime XML", "Import Manga XML",
	                                          "Export Anime XML", "Export Manga XML"};
	int choice = 0;
	do {
		ClearScreen(app);
		DrawMenu(app, crumb, options);
		choice = ReadChoice(app, static_cast<int>(options.size()));
		std::string heading = choice > 0 ? crumb + " / " + options[choice - 1] : "";
		bool acted = false;
		switch (choice) {
			case 1:
				acted = ImportFile(app, MediaType::kAnime, heading);
				break;
			case 2:
				acted = ImportFile(app, MediaType::kManga, heading);
				break;
			case 3:
				acted = ExportList(app, MediaType::kAnime, heading);
				break;
			case 4:
				acted = ExportList(app, MediaType::kManga, heading);
				break;
			default:
				break;
		}
		if (acted)
			Pause(app);
	} while (choice != 0);
}

}  // namespace catalist
