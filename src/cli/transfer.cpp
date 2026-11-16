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

// A list read whole, before anything is added, and what to call it.
struct source {
	std::string name;
	std::vector<entry> titles;
};

// Adds the titles of each source to the lists, as import does: a title
// there already keeps what the person gave it. One step for undo.
int addSources(cliContext& ctx, const args::parsed& line, const std::vector<source>& sources) {
	if (not loadForWrite(ctx))
		return 1;
	std::vector<entry> animeBefore = ctx.db.getEntries(mediaType::kAnime);
	std::vector<entry> mangaBefore = ctx.db.getEntries(mediaType::kManga);
	for (const auto& each : sources) {
		size_t added = 0;
		for (const auto& title : each.titles) {
			size_t before = ctx.db.getEntries(title.type).size();
			ctx.db.addEntry(title);
			added += ctx.db.getEntries(title.type).size() - before;
		}
		if (not line.has("q"))
			ctx.err << fmt::format("{}: {}, {} added, {} in your lists already\n", each.name,
			                       titles(each.titles.size()), added, each.titles.size() - added);
	}
	auto changes = journal::diff(animeBefore, ctx.db.getEntries(mediaType::kAnime));
	auto manga = journal::diff(mangaBefore, ctx.db.getEntries(mediaType::kManga));
	changes.insert(changes.end(), manga.begin(), manga.end());
	// An id a file filled in is no step for undo, but it is kept all the same.
	if (changes.empty() and not ctx.db.saveAll())
		return saveFailed(ctx);
	return saveAndReport(ctx, line, changes);
}

// import --user NAME: the user's public lists on a site, both unless -a or
// -m keeps to one.
int importUser(const command& self, const args::parsed& line, cliContext& ctx) {
	if (not line.operands.empty())
		return usageError(ctx, self, "give FILEs or --user, not both");
	if (not fromKnown(self, line, ctx))
		return 2;
	if (asksProvider(line) and not load(ctx))
		return 1;
	auto site = trackerFor(ctx, siteOf(line, ctx));
	std::string user = line.get("user");
	auto only = onlyType(line);
	std::vector<source> sources;
	for (mediaType type : {mediaType::kAnime, mediaType::kManga}) {
		if (only and *only != type)
			continue;
		if (not sources.empty())
			ctx.net.pause();
		auto list = site->pull(type, user);
		if (not list.ok())
			return fail(ctx, webError(list.errorMessage()));
		sources.push_back(
		    {fmt::format("{} on {} ({})", user, providerName(site->site()), typeKey(type)),
		     list.value()});
	}
	return addSources(ctx, line, sources);
}

int runImport(const command& self, const args::parsed& line, cliContext& ctx) {
	if (line.has("user"))
		return importUser(self, line, ctx);
	if (line.has("from") or line.has("a") or line.has("m"))
		return usageError(ctx, self, "--from, -a and -m go with --user");
	if (line.operands.empty())
		return usageError(ctx, self, "give at least one file, or - for standard input");
	// Everything is read before anything is added, so a bad file leaves the
	// catalogue untouched.
	std::vector<source> sources;
	for (const auto& file : line.operands) {
		auto read = readExport(ctx, file);
		if (not read.ok())
			return fail(ctx, read.errorMessage());
		sources.push_back({file == "-" ? "standard input" : file, read.value()});
	}
	return addSources(ctx, line, sources);
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
	// The XML is one list a file, as the import pages take it; csv and json
	// hold both lists when none is named.
	bool both = line.operands.empty() and format != "xml";
	if (not named and not both)
		return usageError(ctx, self,
		                  format == "xml" ? "say which list: anime or manga (XML is one a file)"
		                                  : "say which list: anime, manga, or none for both");
	if (not load(ctx))
		return 1;

	std::vector<entry> list;
	for (mediaType type : {mediaType::kAnime, mediaType::kManga}) {
		if (both or line.operands[0] == typeKey(type)) {
			const auto& more = ctx.db.getEntries(type);
			list.insert(list.end(), more.begin(), more.end());
		}
	}
	if (format == "xml") {
		mediaType type = line.operands[0] == "anime" ? mediaType::kAnime : mediaType::kManga;
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
	     "Add titles from MyAnimeList XML files, or a user's lists on a site",
	     std::string("A FILE of - is read from standard input. MyAnimeList gzips its exports:\n"
	                 "unpack them first (gunzip). With --user, the user's public lists are\n"
	                 "read from your provider, or the site --from names, both of them unless\n"
	                 "-a or -m keeps to one: moving in from AniList, MyAnimeList or Kitsu.\n\n"
	                 "A title already in your lists keeps your score, progress and dates;\n"
	                 "only its totals and ids are refreshed. Everything is read before\n"
	                 "anything is added: when one source is bad, nothing is changed. The\n"
	                 "titles keep their ids, so they are linked to the sites that know them.\n\n") +
	         kChangeFormat,
	     {{'a', "", "", "with --user: the anime list only"},
	      {'m', "", "", "with --user: the manga list only"},
	      {0, "user", "NAME", "read NAME's public lists on a site, not FILEs"},
	      kFrom,
	      kQuiet},
	     "[FILE...]",
	     runImport,
	     operandKind::kFiles},
	    {"export",
	     {},
	     "Print a list as MyAnimeList XML, CSV or JSON",
	     "The XML is what MyAnimeList and AniList read on their import pages, and\n"
	     "what 'import' reads; titles without a MyAnimeList id are left out of it,\n"
	     "with a note. csv is a line of column names and a line a title, and json\n"
	     "one object a line; both have every title and the columns of 'ls', and\n"
	     "hold both lists when none is named. In the CSV a date that is not set\n"
	     "and a total that is not known are empty.\n",
	     {{0, "format", "FORMAT", "xml (the default), csv or json", {"xml", "csv", "json"}}},
	     "[anime|manga]",
	     runExport,
	     operandKind::kWords,
	     {"anime", "manga"}},
	};
}

}  // namespace catalist::cli
