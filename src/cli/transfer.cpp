#include <fmt/format.h>

#include <algorithm>
#include <istream>
#include <iterator>
#include <ostream>

#include "cli/common.hpp"
#include "storage/malXml.hpp"
#include "system.hpp"

namespace catalist::cli {

namespace {

// The titles of one MyAnimeList XML export, `-` being standard input. Fails
// with what is wrong with the file.
result<std::vector<entry>> readExport(cliContext& ctx, const std::string& file) {
	using outcome = result<std::vector<entry>>;
	bool piped = file == "-";
	std::string name = piped ? "standard input" : file;
	auto content =
	    piped ? std::optional<std::string>(std::string(std::istreambuf_iterator<char>(ctx.in), {}))
	          : readFile(file);
	if (not content)
		return outcome::failure(fmt::format("cannot read {}", name));
	// MyAnimeList hands out its exports gzipped.
	if (content->rfind("\x1f\x8b", 0) == 0)
		return outcome::failure(fmt::format("{} is compressed; unpack it first (gunzip{})", name,
		                                    piped ? "" : " " + file));
	auto found = malXml::parse(*content);
	if (found.empty())
		return outcome::failure(
		    fmt::format("no anime or manga found in {} (is it a MyAnimeList XML export?)", name));
	return outcome::success(std::move(found));
}

int runImport(const command& self, const args::parsed& line, cliContext& ctx) {
	if (line.operands.empty())
		return usageError(ctx, self, "give at least one file, or - for standard input");
	// Everything is read before anything is added, so a bad file leaves the
	// catalogue untouched.
	std::vector<std::vector<entry>> lists;
	for (const auto& file : line.operands) {
		auto read = readExport(ctx, file);
		if (not read.ok())
			return fail(ctx, read.errorMessage());
		lists.push_back(read.value());
	}
	if (not loadForWrite(ctx))
		return 1;

	std::vector<entry> animeBefore = ctx.db.getEntries(mediaType::kAnime);
	std::vector<entry> mangaBefore = ctx.db.getEntries(mediaType::kManga);
	for (size_t i = 0; i < lists.size(); ++i) {
		size_t added = 0;
		for (const auto& title : lists[i]) {
			size_t before = ctx.db.getEntries(title.type).size();
			ctx.db.addEntry(title);
			added += ctx.db.getEntries(title.type).size() - before;
		}
		if (not line.has("q"))
			ctx.err << fmt::format("{}: {}, {} added, {} in your lists already\n",
			                       line.operands[i] == "-" ? "standard input" : line.operands[i],
			                       titles(lists[i].size()), added, lists[i].size() - added);
	}
	auto changes = journal::diff(animeBefore, ctx.db.getEntries(mediaType::kAnime));
	auto manga = journal::diff(mangaBefore, ctx.db.getEntries(mediaType::kManga));
	changes.insert(changes.end(), manga.begin(), manga.end());
	// An id a file filled in is no step for undo, but it is kept all the same.
	if (changes.empty() and not ctx.db.saveAll())
		return saveFailed(ctx);
	return saveAndReport(ctx, line, changes);
}

int runExport(const command& self, const args::parsed& line, cliContext& ctx) {
	static const std::vector<std::string> kFormats = {"xml", "csv", "json"};
	std::string format = line.has("format") ? line.get("format") : "xml";
	if (std::find(kFormats.begin(), kFormats.end(), format) == kFormats.end())
		return usageError(ctx, self,
		                  fmt::format("unknown format '{}'{}; choose xml, csv or json", format,
		                              didYouMean(format, kFormats)));
	bool named = line.operands.size() == 1 and std::find(self.words.begin(), self.words.end(),
	                                                     line.operands[0]) != self.words.end();
	if (not named)
		return usageError(ctx, self, "say which list: anime or manga");
	mediaType type = line.operands[0] == "anime" ? mediaType::kAnime : mediaType::kManga;
	if (not load(ctx))
		return 1;

	const auto& list = ctx.db.getEntries(type);
	if (format == "xml") {
		auto written = malXml::write(list, type);
		ctx.out << written.xml;
		if (written.skipped > 0)
			ctx.err << fmt::format("note: {} without a MyAnimeList id {} left out.\n",
			                       titles(written.skipped), written.skipped == 1 ? "was" : "were");
		return 0;
	}
	std::vector<std::vector<output::cell>> rows;
	for (const auto& e : list)
		rows.push_back(cellsOf(e));
	if (format == "json") {
		writeRows(ctx, kColumns, rows, true, false);
		return 0;
	}
	ctx.out << output::csvLine(kColumns);
	for (size_t i = 0; i < rows.size() and ctx.out; ++i)
		ctx.out << output::csvLine(rows[i]);
	return 0;
}

}  // namespace

std::vector<command> transferCommands() {
	return {
	    {"import",
	     {},
	     "Add the titles of MyAnimeList XML exports",
	     std::string("A FILE of - is read from standard input. A title already in your lists\n"
	                 "keeps your score, progress and dates; only its totals are refreshed.\n"
	                 "Every file is read before anything is added: when one of them is bad,\n"
	                 "nothing is changed. MyAnimeList gzips its exports: unpack them first\n"
	                 "(gunzip).\n\n") +
	         kChangeFormat,
	     {kQuiet},
	     "FILE...",
	     runImport,
	     operandKind::kFiles},
	    {"export",
	     {},
	     "Print a list as MyAnimeList XML, CSV or JSON",
	     "The XML is what MyAnimeList and AniList read on their import pages, and\n"
	     "what 'import' reads; titles without a MyAnimeList id are left out of it,\n"
	     "with a note. csv is a line of column names and a line a title, and json\n"
	     "one object a line; both have every title and the columns of 'ls'. In the\n"
	     "CSV a date that is not set and a total that is not known are empty.\n",
	     {{0, "format", "FORMAT", "xml (the default), csv or json", {"xml", "csv", "json"}}},
	     "anime|manga",
	     runExport,
	     operandKind::kWords,
	     {"anime", "manga"}},
	};
}

}  // namespace catalist::cli
