#include <algorithm>
#include <filesystem>
#include <format>
#include <ostream>
#include <sstream>

#include "cli/common.hpp"
#include "cli/refs.hpp"
#include "core/text.hpp"
#include "system.hpp"

namespace catalist::cli {

namespace {

struct StatusChoice {
	const char* label;
	const char* word;
};

constexpr StatusChoice kStatusChoices[] = {
    {"Planned", "planned"},   {"Watching or reading", "watching"},
    {"Completed", "completed"}, {"On hold", "on-hold"},
    {"Dropped", "dropped"},   {"Rewatching or rereading", "rewatching"},
};
constexpr int kStatusCount = 6;
constexpr int kStatusesForNewTitle = 5;
constexpr size_t kHomeRows = 5;

std::string Trimmed(const std::string& text) {
	auto first = text.find_first_not_of(" \t\r");
	if (first == std::string::npos)
		return "";
	return text.substr(first, text.find_last_not_of(" \t\r") - first + 1);
}

std::string LabelOf(const std::vector<std::string>& args, const Entry* title) {
	std::string label;
	for (const auto& word : args) {
		if (word == "--")
			continue;
		bool is_ref = title and word == refs::Of(*title);
		label += (label.empty() ? "" : " ") + (is_ref ? DisplayName(*title) : word);
	}
	return label;
}

int PerformInto(CliContext& ctx, std::ostream& out, const std::vector<std::string>& args,
                const Entry* title) {
	// A command that stops early holds its write lock until its catalogue goes
	// away, so each one gets its own.
	Catalogue db(ctx.db.data_dir());
	CliContext run{db, ctx.in, out, ctx.err, ctx.clock, ctx.net};
	run.interactive = ctx.interactive;
	run.color = ctx.color;
	run.columns = &out == &ctx.out ? ctx.columns : 0;
	run.rows = ctx.rows;
	run.pager = ctx.pager;
	run.label = LabelOf(args, title);
	ctx.out.flush();
	int code = RunCli(args, run);
	out.flush();
	return code;
}

int Perform(CliContext& ctx, const std::vector<std::string>& args, const Entry* title = nullptr) {
	return PerformInto(ctx, ctx.out, args, title);
}

std::optional<std::string> AskLine(CliContext& ctx, const std::string& prompt) {
	ctx.out << prompt << std::flush;
	std::string answer;
	if (not std::getline(ctx.in, answer)) {
		ctx.out << "\n";
		return std::nullopt;
	}
	return Trimmed(answer);
}

std::optional<std::string> AskText(CliContext& ctx, const std::string& prompt) {
	auto answer = AskLine(ctx, prompt);
	return answer and not answer->empty() ? answer : std::nullopt;
}

bool AskYes(CliContext& ctx, const std::string& question) {
	auto answer = AskLine(ctx, std::format("{} [y/N]: ", question));
	if (not answer)
		return false;
	std::string word = utf8::Lowercase(*answer);
	return word == "y" or word == "yes";
}

int ReadChoice(CliContext& ctx, int highest) {
	while (true) {
		auto answer = AskLine(ctx, std::format("Choose [0-{}]: ", highest));
		if (not answer)
			return 0;
		auto number = args::ParseCount(*answer);
		if (number and *number <= highest)
			return *number;
		ctx.out << std::format("Enter a number from 0 to {}.\n", highest);
	}
}

void Pause(CliContext& ctx) {
	(void)AskLine(ctx, "\nPress Enter to continue...");
}

void ClearScreen(CliContext& ctx) {
	if (ctx.clear_screen)
		ctx.out << "\x1b[H\x1b[2J";
}

void DrawMenu(CliContext& ctx, const std::string& title, const std::vector<std::string>& items,
              const std::string& leave = "Back") {
	ctx.out << output::Paint(title, output::kBold, ctx.color) << "\n\n";
	for (size_t i = 0; i < items.size(); ++i)
		ctx.out << std::format("  {:>2}  {}\n", i + 1, items[i]);
	ctx.out << std::format("  {:>2}  {}\n\n", 0, leave);
}

std::optional<std::string> AskStatus(CliContext& ctx, const std::string& title, int count) {
	std::vector<std::string> labels;
	for (int i = 0; i < count; ++i)
		labels.emplace_back(kStatusChoices[i].label);
	DrawMenu(ctx, title, labels);
	int choice = ReadChoice(ctx, count);
	if (choice == 0)
		return std::nullopt;
	return kStatusChoices[choice - 1].word;
}

std::optional<Entry> ChooseFrom(CliContext& ctx, std::vector<Entry> titles) {
	SortEntries(titles, SortKey::kUpdated, false);
	size_t more = titles.size() > kMostOffered ? titles.size() - kMostOffered : 0;
	titles.resize(titles.size() - more);
	ctx.out.flush();
	PrintChoices(ctx, titles);
	ctx.err.flush();
	if (more > 0)
		ctx.out << std::format("  and {} more: type more of the name to reach them\n", more);
	ctx.out << "   0  Back\n\n";
	int choice = ReadChoice(ctx, static_cast<int>(titles.size()));
	if (choice == 0)
		return std::nullopt;
	return titles[static_cast<size_t>(choice - 1)];
}

std::optional<Entry> ChooseTitle(CliContext& ctx) {
	while (true) {
		auto text = AskText(ctx, "Part of the title (Enter to go back): ");
		if (not text)
			return std::nullopt;
		Catalogue db(ctx.db.data_dir());
		auto loaded = db.LoadAll();
		if (not loaded.has_value()) {
			Fail(ctx, loaded.error());
			return std::nullopt;
		}
		auto found = refs::Find(db, *text, std::nullopt);
		if (found.size() == 1)
			return found[0];
		if (not found.empty()) {
			if (auto chosen = ChooseFrom(ctx, found))
				return chosen;
			continue;
		}
		auto closest = refs::Closest(db, *text, std::nullopt);
		if (closest and AskYes(ctx, std::format("Did you mean {}?", DisplayName(*closest))))
			return closest;
		if (not closest)
			ctx.out << std::format("Nothing in your lists matches '{}'.\n", *text);
	}
}

void AddTitle(CliContext& ctx) {
	ClearScreen(ctx);
	ctx.out << output::Paint("ADD A TITLE", output::kBold, ctx.color) << "\n\n";
	auto name = AskText(ctx, "Title to find on AniList (Enter to go back): ");
	if (not name)
		return;
	ctx.out << "\n";
	auto status = AskStatus(ctx, "Add it as", kStatusesForNewTitle);
	if (not status)
		return;
	Perform(ctx, {"add", std::format("--status={}", *status), "--", *name});
	Pause(ctx);
}

void ListByStatus(CliContext& ctx) {
	ctx.out << "\n";
	if (auto status = AskStatus(ctx, "Which status?", kStatusCount))
		Perform(ctx, {"ls", "-s", *status});
}

void ViewLists(CliContext& ctx) {
	int choice = 0;
	do {
		ClearScreen(ctx);
		DrawMenu(ctx, "VIEW MY LISTS",
		         {"In progress", "All titles", "Anime only", "Manga only", "By status",
		          "Find by name", "Full details of a title"});
		choice = ReadChoice(ctx, 7);
		switch (choice) {
			case 1:
				Perform(ctx, {"ls", "-s", "watching,rewatching", "--sort=updated"});
				break;
			case 2:
				Perform(ctx, {"ls"});
				break;
			case 3:
				Perform(ctx, {"ls", "-a"});
				break;
			case 4:
				Perform(ctx, {"ls", "-m"});
				break;
			case 5:
				ListByStatus(ctx);
				break;
			case 6:
				if (auto text = AskText(ctx, "Words in the title (Enter to go back): "))
					Perform(ctx, {"ls", "--", *text});
				break;
			case 7:
				if (auto title = ChooseTitle(ctx))
					Perform(ctx, {"show", refs::Of(*title)}, &*title);
				break;
			default:
				break;
		}
		if (choice != 0)
			Pause(ctx);
	} while (choice != 0);
}

void CountNext(CliContext& ctx) {
	ClearScreen(ctx);
	ctx.out << output::Paint("COUNT THE NEXT EPISODE OR CHAPTER", output::kBold, ctx.color)
	        << "\n\n";
	Catalogue db(ctx.db.data_dir());
	auto loaded = db.LoadAll();
	if (not loaded.has_value()) {
		Fail(ctx, loaded.error());
		Pause(ctx);
		return;
	}
	std::vector<Entry> open;
	for (MediaType type : {MediaType::kAnime, MediaType::kManga})
		for (const auto& each : db.GetEntries(type))
			if (each.status == WatchStatus::kWatching or each.status == WatchStatus::kRewatching)
				open.push_back(each);
	if (open.empty()) {
		ctx.out << "Nothing is in progress.\n";
		Pause(ctx);
		return;
	}
	auto title = ChooseFrom(ctx, open);
	if (not title)
		return;
	Perform(ctx, {"+", refs::Of(*title)}, &*title);
	Pause(ctx);
}

void AskCount(CliContext& ctx, const Entry& title, const std::string& field,
              const std::string& prompt, std::optional<int> highest = std::nullopt) {
	auto text = AskText(ctx, std::format("{} (Enter to go back): ", prompt));
	if (not text)
		return;
	auto number = args::ParseCount(*text);
	if (not number or (highest and *number > *highest)) {
		ctx.out << (highest ? std::format("Enter a number from 0 to {}.\n", *highest)
		                    : std::string("Enter a whole number.\n"));
		return;
	}
	Perform(ctx, {"set", refs::Of(title), std::format("{}={}", field, *number)}, &title);
}

void AskDate(CliContext& ctx, const Entry& title, const std::string& field) {
	auto text = AskText(ctx, "Date as YYYY-MM-DD, today, or none (Enter to go back): ");
	if (not text)
		return;
	std::string word = utf8::Lowercase(*text);
	if (word != "today" and word != "none" and not IsoDate::Parse(word)) {
		ctx.out << "Enter a real date such as 2026-10-20.\n";
		return;
	}
	std::string value = word == "none" ? "" : word;
	Perform(ctx, {"set", refs::Of(title), std::format("{}={}", field, value)}, &title);
}

void EditTitle(CliContext& ctx) {
	ClearScreen(ctx);
	ctx.out << output::Paint("EDIT A TITLE", output::kBold, ctx.color) << "\n\n";
	auto chosen = ChooseTitle(ctx);
	if (not chosen)
		return;
	std::string ref = refs::Of(*chosen);
	int choice = 0;
	do {
		ClearScreen(ctx);
		if (Perform(ctx, {"show", ref}, &*chosen) != 0)
			return;
		ctx.out << "\n";
		DrawMenu(ctx, "EDIT",
		         {"Count one more", "Count one less", "Set progress", "Set score",
		          "Change status", "Set total", "Set volumes (manga)", "Set start date",
		          "Set finish date", "Rename"});
		choice = ReadChoice(ctx, 10);
		switch (choice) {
			case 1:
				Perform(ctx, {"+", ref}, &*chosen);
				break;
			case 2:
				Perform(ctx, {"-", ref}, &*chosen);
				break;
			case 3:
				AskCount(ctx, *chosen, "progress", "New progress");
				break;
			case 4:
				AskCount(ctx, *chosen, "score", "Score from 0 to 10", 10);
				break;
			case 5:
				ctx.out << "\n";
				if (auto status = AskStatus(ctx, "New status", kStatusCount))
					Perform(ctx, {"set", ref, std::format("status={}", *status)}, &*chosen);
				break;
			case 6:
				AskCount(ctx, *chosen, "total", "New total");
				break;
			case 7:
				if (chosen->type == MediaType::kManga)
					AskCount(ctx, *chosen, "volumes", "Volumes read");
				else
					ctx.out << "Only manga have volumes.\n";
				break;
			case 8:
				AskDate(ctx, *chosen, "start");
				break;
			case 9:
				AskDate(ctx, *chosen, "finish");
				break;
			case 10:
				if (auto name = AskText(ctx, "New name (Enter to go back): "))
					Perform(ctx, {"set", ref, std::format("title={}", *name)}, &*chosen);
				break;
			default:
				break;
		}
		if (choice != 0)
			Pause(ctx);
	} while (choice != 0);
}

void RemoveTitle(CliContext& ctx) {
	ClearScreen(ctx);
	ctx.out << output::Paint("REMOVE A TITLE", output::kBold, ctx.color) << "\n\n";
	auto title = ChooseTitle(ctx);
	if (not title)
		return;
	Perform(ctx, {"rm", refs::Of(*title)}, &*title);
	Pause(ctx);
}

void ReplayStep(CliContext& ctx, const std::string& command) {
	if (Perform(ctx, {command}) != 0 and AskYes(ctx, "Do it anyway?"))
		Perform(ctx, {command, "-f"});
}

void History(CliContext& ctx) {
	int choice = 0;
	do {
		ClearScreen(ctx);
		DrawMenu(ctx, "UNDO AND HISTORY", {"Undo the last change", "Redo", "Show recent changes"});
		choice = ReadChoice(ctx, 3);
		switch (choice) {
			case 1:
				ReplayStep(ctx, "undo");
				break;
			case 2:
				ReplayStep(ctx, "redo");
				break;
			case 3:
				Perform(ctx, {"log", "-n", "20"});
				break;
			default:
				break;
		}
		if (choice != 0)
			Pause(ctx);
	} while (choice != 0);
}

void ExportList(CliContext& ctx, const std::string& list) {
	auto file = AskText(ctx, std::format("File to save the {} list in (Enter to go back): ", list));
	if (not file)
		return;
	std::string path = ExpandHome(*file);
	std::error_code ignored;
	bool taken = std::filesystem::exists(path, ignored);
	if (taken and not AskYes(ctx, std::format("{} exists. Replace it?", path))) {
		ctx.out << "Nothing was saved.\n";
		return;
	}
	std::ostringstream xml;
	if (PerformInto(ctx, xml, {"export", list}, nullptr) != 0)
		return;
	if (WriteFile(path, xml.str()))
		ctx.out << std::format("Saved {}.\n", path);
	else
		Fail(ctx, std::format("could not write {}", path));
}

void Transfer(CliContext& ctx) {
	int choice = 0;
	do {
		ClearScreen(ctx);
		DrawMenu(ctx, "IMPORT OR EXPORT",
		         {"Import a MyAnimeList XML file", "Export anime", "Export manga"});
		choice = ReadChoice(ctx, 3);
		switch (choice) {
			case 1:
				if (auto file = AskText(ctx, "File to import (Enter to go back): "))
					Perform(ctx, {"import", "--", ExpandHome(*file)});
				break;
			case 2:
				ExportList(ctx, "anime");
				break;
			case 3:
				ExportList(ctx, "manga");
				break;
			default:
				break;
		}
		if (choice != 0)
			Pause(ctx);
	} while (choice != 0);
}

void DrawHome(CliContext& ctx) {
	ClearScreen(ctx);
	ctx.out << output::Paint("CATALIST", output::kBold, ctx.color) << "\n";
	Catalogue db(ctx.db.data_dir());
	auto loaded = db.LoadAll();
	if (not loaded.has_value()) {
		Fail(ctx, loaded.error());
		return;
	}
	const auto& anime = db.GetEntries(MediaType::kAnime);
	const auto& manga = db.GetEntries(MediaType::kManga);
	std::vector<Entry> open;
	for (const auto* list : {&anime, &manga})
		for (const auto& each : *list)
			if (each.status == WatchStatus::kWatching or each.status == WatchStatus::kRewatching)
				open.push_back(each);
	ctx.out << std::format("{} anime, {} manga, {} in progress\n\n", anime.size(), manga.size(),
	                       open.size());
	if (open.empty())
		return;
	SortEntries(open, SortKey::kUpdated, false);
	open.resize(std::min(open.size(), kHomeRows));
	std::vector<std::vector<output::Styled>> rows;
	for (const auto& each : open)
		rows.push_back({{DisplayName(each)},
		                {TypeKey(each.type)},
		                {OutOf(each.progress, each.total_units)}});
	ctx.out << output::Sections({{"", false, true}, {""}, {""}}, {{"CONTINUE", rows}},
	                            ctx.columns > 0 ? ctx.columns : 80, ctx.color)
	        << "\n";
}

}  // namespace

int RunMenu(CliContext& ctx) {
	int choice = 0;
	do {
		DrawHome(ctx);
		DrawMenu(ctx, "MAIN MENU",
		         {"Add a title", "View my lists", "Count the next episode or chapter",
		          "Edit a title", "Remove a title", "Statistics", "Undo and history",
		          "Import or export"},
		         "Exit");
		choice = ReadChoice(ctx, 8);
		switch (choice) {
			case 1:
				AddTitle(ctx);
				break;
			case 2:
				ViewLists(ctx);
				break;
			case 3:
				CountNext(ctx);
				break;
			case 4:
				EditTitle(ctx);
				break;
			case 5:
				RemoveTitle(ctx);
				break;
			case 6:
				Perform(ctx, {"stats"});
				Pause(ctx);
				break;
			case 7:
				History(ctx);
				break;
			case 8:
				Transfer(ctx);
				break;
			default:
				break;
		}
	} while (choice != 0);
	ctx.out << "Goodbye.\n";
	return 0;
}

}  // namespace catalist::cli
