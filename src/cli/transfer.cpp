#include <algorithm>
#include <format>
#include <istream>
#include <iterator>
#include <ostream>
#include <set>
#include <string_view>

#include "cli/common.hpp"
#include "storage/mal_xml.hpp"
#include "system.hpp"

namespace catalist::cli {

namespace {

// The titles of one MyAnimeList XML export, `-` being standard input. Fails
// with what is wrong with the file.
Result<std::vector<Entry>> ReadExport(CliContext& ctx, const std::string& file) {
	bool piped = file == "-";
	std::string name = piped ? "standard input" : file;
	auto content =
	    piped ? std::optional<std::string>(std::string(std::istreambuf_iterator<char>(ctx.in), {}))
	          : ReadFile(file);
	if (not content)
		return std::unexpected(std::format("cannot read {}", name));
	// MyAnimeList hands out its exports gzipped.
	if (content->rfind("\x1f\x8b", 0) == 0)
		return std::unexpected(std::format("{} is compressed; unpack it first (gunzip{})", name,
		                                   piped ? "" : " " + file));
	auto found = mal_xml::Parse(*content);
	if (found.empty())
		return std::unexpected(
		    std::format("no anime or manga found in {} (is it a MyAnimeList XML export?)", name));
	return found;
}

// A list read whole, before anything is added, and what to call it.
struct Source {
	std::string name;
	std::vector<Entry> titles;
};

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

void PrintClashes(CliContext& ctx, const Entry& mine, const std::vector<Clash>& clashes) {
	size_t width = 0;
	for (const auto& each : clashes)
		width = std::max(width, each.yours.size());
	ctx.err << std::format("{} ({})\n", DisplayName(mine), TypeKey(mine.type));
	for (const auto& each : clashes)
		ctx.err << std::format("  {:<9} yours {:<{}}   file {}\n", each.field, each.yours, width,
		                       each.file);
	ctx.err << "\n";
}

char AskClash(CliContext& ctx) {
	while (true) {
		ctx.err << "[k]eep  [f]ile  [K]eep rest  [F]ile rest  [q]uit: " << std::flush;
		std::string answer;
		if (not std::getline(ctx.in, answer))
			return 'q';
		if (answer.size() == 1 and std::string_view("kfKFq").contains(answer[0]))
			return answer[0];
	}
}

std::optional<std::set<const Entry*>> AskClashes(CliContext& ctx,
                                                 const std::vector<Source>& sources) {
	std::set<const Entry*> take_file;
	std::optional<char> rest;
	for (const auto& source : sources) {
		for (const auto& title : source.titles) {
			auto mine = ctx.db.FindImported(title);
			auto clashes = mine ? ClashesOf(*mine, title) : std::vector<Clash>{};
			if (clashes.empty())
				continue;
			char answer = rest.value_or('\0');
			if (not rest) {
				PrintClashes(ctx, *mine, clashes);
				answer = AskClash(ctx);
			}
			if (answer == 'q')
				return std::nullopt;
			if (answer == 'K' or answer == 'F')
				rest = answer;
			if (answer == 'f' or answer == 'F')
				take_file.insert(&title);
		}
	}
	return take_file;
}

// Adds the titles of each source to the lists: a title there already keeps
// what the person gave it and gets only what it lacks, unless it is one of
// `take_file`. One step for undo.
int AddSources(CliContext& ctx, const args::Parsed& line, const std::vector<Source>& sources,
               const std::set<const Entry*>& take_file) {
	if (not LoadForWrite(ctx))
		return 1;
	std::vector<Entry> anime_before = ctx.db.GetEntries(MediaType::kAnime);
	std::vector<Entry> manga_before = ctx.db.GetEntries(MediaType::kManga);
	for (const auto& each : sources) {
		size_t added = 0;
		for (const auto& title : each.titles) {
			size_t before = ctx.db.GetEntries(title.type).size();
			int id = ctx.db.ImportEntry(title);
			if (take_file.contains(&title))
				ctx.db.UpdateEntry(TakeImported(*ctx.db.GetById(title.type, id), title));
			added += ctx.db.GetEntries(title.type).size() - before;
		}
		if (not line.Has("q"))
			ctx.err << std::format("{}: {}, {} added, {} in your lists already\n", each.name,
			                       Titles(each.titles.size()), added, each.titles.size() - added);
	}
	auto changes = journal::Diff(anime_before, ctx.db.GetEntries(MediaType::kAnime));
	auto manga = journal::Diff(manga_before, ctx.db.GetEntries(MediaType::kManga));
	changes.insert(changes.end(), manga.begin(), manga.end());
	// An id a file filled in is no step for undo, but it is kept all the same.
	if (changes.empty() and not ctx.db.SaveAll())
		return SaveFailed(ctx);
	return SaveAndReport(ctx, line, changes);
}

int RunImport(const Command& self, const args::Parsed& line, CliContext& ctx) {
	if (line.operands.empty())
		return UsageError(ctx, self, "give at least one file, or - for standard input");
	// Everything is read before anything is added, so a bad file leaves the
	// catalogue untouched.
	std::vector<Source> sources;
	for (const auto& file : line.operands) {
		auto read = ReadExport(ctx, file);
		if (not read.has_value())
			return Fail(ctx, read.error());
		sources.push_back({file == "-" ? "standard input" : file, read.value()});
	}
	std::set<const Entry*> take_file;
	if (ctx.interactive and not line.Has("q")) {
		// Asked before the catalogue is taken for writing: no other program
		// waits while the person chooses.
		if (not Load(ctx))
			return 1;
		auto chosen = AskClashes(ctx, sources);
		if (not chosen) {
			ctx.err << "Cancelled.\n";
			return 1;
		}
		take_file = std::move(*chosen);
	}
	return AddSources(ctx, line, sources, take_file);
}

int RunExport(const Command& self, const args::Parsed& line, CliContext& ctx) {
	bool named =
	    line.operands.size() == 1 and (line.operands[0] == "anime" or line.operands[0] == "manga");
	if (not named)
		return UsageError(ctx, self, "say which list: anime or manga");
	if (not Load(ctx))
		return 1;
	MediaType type = line.operands[0] == "anime" ? MediaType::kAnime : MediaType::kManga;
	auto written = mal_xml::Write(ctx.db.GetEntries(type), type);
	ctx.out << written.xml;
	if (written.skipped > 0)
		ctx.err << std::format("note: {} without a MyAnimeList id {} left out.\n",
		                       Titles(written.skipped), written.skipped == 1 ? "was" : "were");
	return 0;
}

}  // namespace

std::vector<Command> TransferCommands() {
	return {
	    {.name = "import",
	     .summary = "Add titles from MyAnimeList XML files",
	     .details =
	         std::string("A FILE is a MyAnimeList XML export; - is standard input. MyAnimeList\n"
	                     "gzips its exports: unpack them first (gunzip).\n\n"
	                     "An import fills in what a title in your lists lacks, and refreshes its\n"
	                     "totals and ids. Where the file and your lists differ, a terminal asks\n"
	                     "which to keep, title by title; elsewhere, and with -q, yours stay.\n"
	                     "Everything is read before anything is added: when one file is bad, or\n"
	                     "you quit, nothing is changed.\n\n") +
	         kChangeFormat,
	     .options = {kQuiet},
	     .operands = "FILE...",
	     .run = RunImport},
	    {.name = "export",
	     .summary = "Print a list as MyAnimeList XML",
	     .details = "The XML is what the import pages of MyAnimeList and AniList read. Titles\n"
	                "without a MyAnimeList id are left out of it, with a note.\n",
	     .operands = "anime|manga",
	     .run = RunExport},
	};
}

}  // namespace catalist::cli
