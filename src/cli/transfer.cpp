#include <algorithm>
#include <cctype>
#include <format>
#include <istream>
#include <iterator>
#include <ostream>
#include <sstream>

#include "cli/common.hpp"
#include "storage/csvCodec.hpp"
#include "storage/malXml.hpp"
#include "system.hpp"
#include "web/json.hpp"

namespace catalist::cli {

namespace {

int numberOf(const json::value& field) {
	auto number = field.asInteger();
	return number and *number > 0 ? static_cast<int>(*number) : 0;
}

std::optional<isoDate> dateOf(const json::value& field) {
	return isoDate::parse(field.asString().value_or(""));
}

// A title of `export --format=json` (or `show --json`): everything but the
// ref, which the lists give anew. Nothing for a line that is not one.
std::optional<entry> titleOf(const json::value& object) {
	std::string type = object["type"].asString().value_or("");
	auto status = parseStatus(object["status"].asString().value_or(""));
	entry e;
	e.title = object["title"].asString().value_or("");
	if ((type != "anime" and type != "manga") or e.title.empty() or not status)
		return std::nullopt;
	e.type = type == "anime" ? mediaType::kAnime : mediaType::kManga;
	e.status = *status;
	e.score = numberOf(object["score"]);
	e.progress = numberOf(object["progress"]);
	e.totalUnits = numberOf(object["total"]);
	e.volumes = numberOf(object["volumes"]);
	e.rewatched = numberOf(object["rewatched"]);
	e.startDate = dateOf(object["started"]);
	e.finishDate = dateOf(object["finished"]);
	e.updatedAt = dateOf(object["updated"]);
	e.totalVolumes = numberOf(object["total_volumes"]);
	e.released = numberOf(object["released"]);
	e.anilistId = numberOf(object["anilist_id"]);
	e.malId = numberOf(object["mal_id"]);
	e.kitsuId = numberOf(object["kitsu_id"]);
	return e;
}

// The titles of Catalist's own JSON Lines, a title a line. Fails on the
// first line that is not one.
result<std::vector<entry>> readJsonLines(const std::string& content, const std::string& name) {
	std::vector<entry> found;
	std::istringstream lines(content);
	size_t number = 0;
	for (std::string line; std::getline(lines, line);) {
		++number;
		if (line.find_first_not_of(" \t\r") == std::string::npos)
			continue;
		auto object = json::parse(line);
		auto title = object.has_value() ? titleOf(object.value()) : std::nullopt;
		if (not title)
			return std::unexpected(std::format(
			    "line {} of {} is not a title as 'export --format=json' writes one", number, name));
		found.push_back(*title);
	}
	return found;
}

// The titles of one MyAnimeList XML export, of Catalist's own JSON Lines,
// or of an anime.csv or manga.csv of Catalist 0.3.0 and earlier, `-` being
// standard input. Fails with what is wrong with the file.
result<std::vector<entry>> readExport(cliContext& ctx, const std::string& file) {
	bool piped = file == "-";
	std::string name = piped ? "standard input" : file;
	auto content =
	    piped ? std::optional<std::string>(std::string(std::istreambuf_iterator<char>(ctx.in), {}))
	          : readFile(file);
	if (not content)
		return std::unexpected(std::format("cannot read {}", name));
	// MyAnimeList hands out its exports gzipped.
	if (content->rfind("\x1f\x8b", 0) == 0)
		return std::unexpected(std::format("{} is compressed; unpack it first (gunzip{})", name,
		                                   piped ? "" : " " + file));
	// `export --format=json`: a full backup, ids and all.
	auto first = content->find_first_not_of(" \t\r\n");
	if (first != std::string::npos and (*content)[first] == '{')
		return readJsonLines(*content, name);
	// The lists of Catalist 0.3.0 and earlier: a "#id,..." line, then a row
	// a title.
	if (first != std::string::npos and
	    ((*content)[first] == '#' or std::isdigit(static_cast<unsigned char>((*content)[first])))) {
		auto rows = csvCodec::decodeEntries(*content);
		if (not rows)
			return std::unexpected(std::format("{}: {}", name, rows.error()));
		return rows;
	}
	auto found = malXml::parse(*content);
	if (found.empty())
		return std::unexpected(
		    std::format("no anime or manga found in {} (is it a MyAnimeList XML export?)", name));
	return found;
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
			ctx.err << std::format("{}: {}, {} added, {} in your lists already\n", each.name,
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
		if (not list.has_value())
			return fail(ctx, webError(list.error()));
		sources.push_back(
		    {std::format("{} on {} ({})", user, providerName(site->site()), typeKey(type)),
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
		if (not read.has_value())
			return fail(ctx, read.error());
		sources.push_back({file == "-" ? "standard input" : file, read.value()});
	}
	return addSources(ctx, line, sources);
}

int runExport(const command& self, const args::parsed& line, cliContext& ctx) {
	static const std::vector<std::string> kFormats = {"xml", "csv", "json"};
	std::string format = line.has("format") ? line.get("format") : "xml";
	if (std::find(kFormats.begin(), kFormats.end(), format) == kFormats.end())
		return usageError(ctx, self,
		                  std::format("unknown format '{}'{}; choose xml, csv or json", format,
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
			ctx.err << std::format("note: {} without a MyAnimeList id {} left out.\n",
			                       titles(written.skipped), written.skipped == 1 ? "was" : "were");
		return 0;
	}
	// Everything about each title, its ids too: a backup `import` reads back.
	std::vector<std::vector<output::cell>> rows;
	for (const auto& e : list)
		rows.push_back(fullCellsOf(e, ctx.db.getProvider()));
	if (format == "json") {
		writeRows(ctx, allColumns(), rows, true, false);
		return 0;
	}
	ctx.out << output::csvLine(allColumns());
	for (size_t i = 0; i < rows.size() and ctx.out; ++i)
		ctx.out << output::csvLine(rows[i]);
	return 0;
}

}  // namespace

std::vector<command> transferCommands() {
	return {
	    {.name = "import",
	     .summary = "Add titles from MyAnimeList XML files, or a user's lists on a site",
	     .details =
	         std::string(
	             "A FILE is a MyAnimeList XML export, what 'export --format=json'\n"
	             "wrote, or the anime.csv or manga.csv of Catalist 0.3.0 and earlier\n"
	             "(in ~/.config/catalist); - is standard input. MyAnimeList gzips its\n"
	             "exports: unpack them first (gunzip).\n\n"
	             "With --user, NAME's public lists are read from your provider, or the\n"
	             "site --from names, both of them unless -a or -m keeps to one: moving\n"
	             "in from AniList, MyAnimeList or Kitsu.\n\n"
	             "A title already in your lists keeps your score, progress and dates;\n"
	             "only its totals and ids are refreshed. Everything is read before\n"
	             "anything is added: when one source is bad, nothing is changed. The\n"
	             "titles keep their ids, so they are linked to the sites that know them.\n\n") +
	         kChangeFormat,
	     .options = {{'a', "", "", "with --user: the anime list only"},
	                 {'m', "", "", "with --user: the manga list only"},
	                 {0, "user", "NAME", "read NAME's public lists on a site, not FILEs"},
	                 kFrom,
	                 kQuiet},
	     .operands = "[FILE...]",
	     .run = runImport,
	     .completes = operandKind::kFiles},
	    {.name = "export",
	     .summary = "Print a list as MyAnimeList XML, CSV or JSON",
	     .details = "The XML is what the import pages of MyAnimeList, AniList and Kitsu read;\n"
	                "titles without a MyAnimeList id are left out of it, with a note. csv is\n"
	                "a line of column names and a line a title, and json one object a line;\n"
	                "both have every title and the columns of 'show', every site's id among\n"
	                "them, and hold both lists when none is named. The json is a backup:\n"
	                "'import' reads it back whole. In the CSV a date that is not set and a\n"
	                "total or an id that is not known are empty.\n",
	     .options =
	         {{0, "format", "FORMAT", "xml (the default), csv or json", {"xml", "csv", "json"}}},
	     .operands = "[anime|manga]",
	     .run = runExport,
	     .completes = operandKind::kWords,
	     .words = {"anime", "manga"}},
	};
}

}  // namespace catalist::cli
