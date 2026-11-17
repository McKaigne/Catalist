#include <algorithm>
#include <cctype>
#include <format>
#include <istream>
#include <iterator>
#include <ostream>
#include <sstream>

#include "cli/common.hpp"
#include "storage/csv_codec.hpp"
#include "storage/mal_xml.hpp"
#include "system.hpp"
#include "web/json.hpp"

namespace catalist::cli {

namespace {

std::optional<IsoDate> DateOf(const json::Value& field) {
	return IsoDate::Parse(field.AsString().value_or(""));
}

// A title of `export --format=json` (or `show --json`): everything but the
// ref, which the lists give anew. Nothing for a line that is not one.
std::optional<Entry> TitleOf(const json::Value& object) {
	std::string type = object["type"].AsString().value_or("");
	auto status = ParseStatus(object["status"].AsString().value_or(""));
	Entry e;
	e.title = object["title"].AsString().value_or("");
	if ((type != "anime" and type != "manga") or e.title.empty() or not status)
		return std::nullopt;
	e.type = type == "anime" ? MediaType::kAnime : MediaType::kManga;
	e.status = *status;
	e.score = object["score"].AsCount();
	e.progress = object["progress"].AsCount();
	e.total_units = object["total"].AsCount();
	e.volumes = object["volumes"].AsCount();
	e.rewatched = object["rewatched"].AsCount();
	e.start_date = DateOf(object["started"]);
	e.finish_date = DateOf(object["finished"]);
	e.updated_at = DateOf(object["updated"]);
	e.total_volumes = object["total_volumes"].AsCount();
	e.released = object["released"].AsCount();
	e.anilist_id = object["anilist_id"].AsCount();
	e.mal_id = object["mal_id"].AsCount();
	e.kitsu_id = object["kitsu_id"].AsCount();
	return e;
}

// The titles of Catalist's own JSON Lines, a title a line. Fails on the
// first line that is not one.
Result<std::vector<Entry>> ReadJsonLines(const std::string& content, const std::string& name) {
	std::vector<Entry> found;
	std::istringstream lines(content);
	size_t number = 0;
	for (std::string line; std::getline(lines, line);) {
		++number;
		if (line.find_first_not_of(" \t\r") == std::string::npos)
			continue;
		auto object = json::Parse(line);
		auto title = object.has_value() ? TitleOf(object.value()) : std::nullopt;
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
	// `export --format=json`: a full backup, ids and all.
	auto first = content->find_first_not_of(" \t\r\n");
	if (first != std::string::npos and (*content)[first] == '{')
		return ReadJsonLines(*content, name);
	// The lists of Catalist 0.3.0 and earlier: a "#id,..." line, then a row
	// a title.
	if (first != std::string::npos and
	    ((*content)[first] == '#' or std::isdigit(static_cast<unsigned char>((*content)[first])))) {
		auto rows = csv_codec::DecodeEntries(*content);
		if (not rows)
			return std::unexpected(std::format("{}: {}", name, rows.error()));
		return rows;
	}
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

// Adds the titles of each source to the lists, as import does: a title
// there already keeps what the person gave it, and gets only what it lacks.
// One step for undo.
int AddSources(CliContext& ctx, const args::Parsed& line, const std::vector<Source>& sources) {
	if (not LoadForWrite(ctx))
		return 1;
	std::vector<Entry> anime_before = ctx.db.GetEntries(MediaType::kAnime);
	std::vector<Entry> manga_before = ctx.db.GetEntries(MediaType::kManga);
	for (const auto& each : sources) {
		size_t added = 0;
		for (const auto& title : each.titles) {
			size_t before = ctx.db.GetEntries(title.type).size();
			ctx.db.AddEntry(title);
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

// import --user NAME: the user's public lists on a site, both unless -a or
// -m keeps to one.
int ImportUser(const Command& self, const args::Parsed& line, CliContext& ctx) {
	if (not line.operands.empty())
		return UsageError(ctx, self, "give FILEs or --user, not both");
	if (not FromKnown(self, line, ctx))
		return 2;
	if (not Load(ctx))
		return 1;
	SendWaiting(ctx);
	auto site = TrackerFor(ctx, SiteOf(line, ctx));
	std::string user = line.Get("user");
	auto only = OnlyType(line);
	std::vector<Source> sources;
	for (MediaType type : {MediaType::kAnime, MediaType::kManga}) {
		if (only and *only != type)
			continue;
		if (not sources.empty())
			ctx.net.pause();
		auto list = site->Pull(type, user);
		if (not list.has_value())
			return Fail(ctx, WebError(list.error()));
		sources.push_back(
		    {std::format("{} on {} ({})", user, ProviderName(site->Site()), TypeKey(type)),
		     list.value()});
	}
	return AddSources(ctx, line, sources);
}

int RunImport(const Command& self, const args::Parsed& line, CliContext& ctx) {
	if (line.Has("user"))
		return ImportUser(self, line, ctx);
	if (line.Has("from") or line.Has("a") or line.Has("m"))
		return UsageError(ctx, self, "--from, -a and -m go with --user");
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
	static const std::vector<std::string> kFormats = {"xml", "csv", "json"};
	std::string format = line.Has("format") ? line.Get("format") : "xml";
	if (std::find(kFormats.begin(), kFormats.end(), format) == kFormats.end())
		return UsageError(ctx, self,
		                  std::format("unknown format '{}'{}; choose xml, csv or json", format,
		                              DidYouMean(format, kFormats)));
	bool named = line.operands.size() == 1 and std::find(self.words.begin(), self.words.end(),
	                                                     line.operands[0]) != self.words.end();
	// The XML is one list a file, as the import pages take it; csv and json
	// hold both lists when none is named.
	bool both = line.operands.empty() and format != "xml";
	if (not named and not both)
		return UsageError(ctx, self,
		                  format == "xml" ? "say which list: anime or manga (XML is one a file)"
		                                  : "say which list: anime, manga, or none for both");
	if (not Load(ctx))
		return 1;

	std::vector<Entry> list;
	for (MediaType type : {MediaType::kAnime, MediaType::kManga}) {
		if (both or line.operands[0] == TypeKey(type)) {
			const auto& more = ctx.db.GetEntries(type);
			list.insert(list.end(), more.begin(), more.end());
		}
	}
	if (format == "xml") {
		MediaType type = line.operands[0] == "anime" ? MediaType::kAnime : MediaType::kManga;
		auto written = mal_xml::Write(list, type);
		ctx.out << written.xml;
		if (written.skipped > 0)
			ctx.err << std::format("note: {} without a MyAnimeList id {} left out.\n",
			                       Titles(written.skipped), written.skipped == 1 ? "was" : "were");
		return 0;
	}
	// Everything about each title, its ids too: a backup `import` reads back.
	std::vector<std::vector<output::Cell>> rows;
	for (const auto& e : list)
		rows.push_back(FullCellsOf(e, ctx.db.provider()));
	if (format == "json") {
		WriteRows(ctx, AllColumns(), rows, true, false);
		return 0;
	}
	ctx.out << output::CsvLine(AllColumns());
	for (size_t i = 0; i < rows.size() and ctx.out; ++i)
		ctx.out << output::CsvLine(rows[i]);
	return 0;
}

}  // namespace

std::vector<Command> TransferCommands() {
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
	             "A title already in your lists keeps the score, progress and dates you\n"
	             "gave it; the import fills in only those still empty, and refreshes its\n"
	             "totals and ids. Everything is read before\n"
	             "anything is added: when one source is bad, nothing is changed. The\n"
	             "titles keep their ids, so they are linked to the sites that know them.\n\n") +
	         kChangeFormat,
	     .options = {{'a', "", "", "with --user: the anime list only"},
	                 {'m', "", "", "with --user: the manga list only"},
	                 {0, "user", "NAME", "read NAME's public lists on a site, not FILEs"},
	                 kFrom,
	                 kQuiet},
	     .operands = "[FILE...]",
	     .run = RunImport,
	     .completes = OperandKind::kFiles},
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
	     .run = RunExport,
	     .completes = OperandKind::kWords,
	     .words = {"anime", "manga"}},
	};
}

}  // namespace catalist::cli
