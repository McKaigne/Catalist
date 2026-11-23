#include <format>
#include <istream>
#include <iterator>
#include <ostream>

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

// Adds the titles of each source to the lists: a title there already keeps
// what the person gave it, and gets only what it lacks. One step for undo.
int AddSources(CliContext& ctx, const args::Parsed& line, const std::vector<Source>& sources) {
	if (not LoadForWrite(ctx))
		return 1;
	std::vector<Entry> anime_before = ctx.db.GetEntries(MediaType::kAnime);
	std::vector<Entry> manga_before = ctx.db.GetEntries(MediaType::kManga);
	for (const auto& each : sources) {
		size_t added = 0;
		for (const auto& title : each.titles) {
			size_t before = ctx.db.GetEntries(title.type).size();
			ctx.db.ImportEntry(title);
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
	return AddSources(ctx, line, sources);
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
	                     "A title already in your lists keeps the score, progress and dates you\n"
	                     "gave it; the import fills in only those still empty, and refreshes its\n"
	                     "totals and ids. Everything is read before anything is added: when one\n"
	                     "file is bad, nothing is changed.\n\n") +
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
