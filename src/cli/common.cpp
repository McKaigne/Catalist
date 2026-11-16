#include "cli/common.hpp"

#include <algorithm>
#include <format>
#include <istream>
#include <ostream>

#include "cli/refs.hpp"
#include "core/text.hpp"
#include "storage/accounts.hpp"
#include "system.hpp"
#include "web/sites.hpp"

namespace catalist::cli {

namespace {

using output::paint;

// How a save of this run is entered in the undo history: under the command
// as typed, cut to 60 characters.
txNote undoNote(const cliContext& ctx) {
	return {utf8::clip(ctx.typed, 60), ctx.stamp()};
}

void sayNoMatch(cliContext& ctx, const std::string& ref, const naming& how) {
	std::string where = how.only ? std::format("your {} list", typeKey(*how.only)) : "your lists";
	auto meant = refs::closest(ctx.db, ref, how.only);
	std::string hint =
	    meant ? std::format(" (did you mean '{}', {}?)", meant->title, refs::of(*meant)) : "";
	fail(ctx, refs::parse(ref) ? std::format("no title in {} has the ref {}", where, ref)
	                           : std::format("nothing in {} matches '{}'{}", where, ref, hint));
	if (ref.size() > 1 and ref[0] == '-')
		ctx.err << "Options go before the first operand.\n";
	ctx.err << "See what is there with: catalist ls\n";
}

// The titles one REF names: one, or all it matches with --all. Nothing,
// with the reason on standard error, when it matches none or when the
// choice among several is not made.
std::optional<std::vector<entry>> named(cliContext& ctx, const std::string& ref,
                                        const naming& how) {
	auto found = refs::find(ctx.db, ref, how.only);
	if (found.empty()) {
		sayNoMatch(ctx, ref, how);
		return std::nullopt;
	}
	if (found.size() == 1 or how.all)
		return found;

	ctx.err << std::format("{}'{}' matches {} titles:\n", how.ask ? "" : "error: ", ref,
	                       found.size());
	for (size_t i = 0; i < found.size(); ++i)
		ctx.err << (how.ask ? std::format("  {:>2}", i + 1) : "") << "  " << refs::of(found[i])
		        << "  " << found[i].title << "\n";
	if (not how.ask) {
		ctx.err << "Name one by its ref (-a and -m keep to one list), or pass --all for all of "
		           "them.\n";
		return std::nullopt;
	}
	auto picked = askWhich(ctx, found.size());
	if (not picked) {
		ctx.err << "Cancelled.\n";
		return std::nullopt;
	}
	return std::vector<entry>{found[*picked]};
}

// One changed field as a terminal shows it: "Odd Taxi  5 → 6/13" for the
// progress, "Odd Taxi  score 8 → 9" for the rest.
std::string changeText(const cliContext& ctx, const entry& shown,
                       const journal::fieldChange& field) {
	std::string title = paint(shown.title, output::kBold, ctx.color);
	if (field.field == "added")
		return std::format("{}  added as {} ({})\n", title, field.after, refs::of(shown));
	if (field.field == "removed")
		return std::format("{}  removed\n", title);
	if (field.field == "progress")
		return std::format("{}  {} → {}\n", title, field.before,
		                   outOf(shown.progress, shown.totalUnits));
	return std::format("{}  {} {} → {}\n", title, paint(field.field, output::kDim, ctx.color),
	                   field.before, field.after);
}

}  // namespace

int fail(cliContext& ctx, const std::string& message) {
	ctx.err << "error: " << message << "\n";
	return 1;
}

int usageError(cliContext& ctx, const command& cmd, const std::string& message) {
	ctx.err << std::format("error: {}\nUsage: catalist {}\nRun 'catalist help {}' for more.\n",
	                       message, usageLine(cmd), cmd.name);
	return 2;
}

std::string didYouMean(const std::string& typed, const std::vector<std::string>& known) {
	auto guess = args::closestTo(typed, known);
	return guess ? std::format(" (did you mean '{}'?)", *guess) : "";
}

int saveFailed(cliContext& ctx) {
	return fail(ctx,
	            std::format("could not save the catalogue to {}", ctx.db.getDataDir().string()));
}

// The columns of a title, in the order every tab-separated line and JSON
// object has them. A new column goes at the end: scripts count on the order.
extern const std::vector<std::string> kColumns = {
    "ref",     "type",      "title",   "status",   "score",   "progress", "total",
    "volumes", "rewatched", "started", "finished", "updated", "new"};

std::vector<output::cell> cellsOf(const entry& e) {
	return {output::text(refs::of(e)),     output::text(typeKey(e.type)),
	        output::text(e.title),         output::text(statusKey(e.status)),
	        output::number(e.score),       output::number(e.progress),
	        output::known(e.totalUnits),   output::number(e.volumes),
	        output::number(e.rewatched),   output::date(e.startDate),
	        output::date(e.finishDate),    output::date(e.updatedAt),
	        output::number(newReleases(e))};
}

extern const std::vector<std::string> kFullColumns = {"total_volumes", "released", "anilist_id",
                                                      "mal_id",        "url",      "kitsu_id"};

std::vector<std::string> allColumns() {
	std::vector<std::string> names = kColumns;
	names.insert(names.end(), kFullColumns.begin(), kFullColumns.end());
	return names;
}

std::vector<output::cell> fullCellsOf(const entry& e, provider site) {
	auto cells = cellsOf(e);
	for (const auto& more :
	     {output::known(e.totalVolumes), output::known(e.released), output::known(e.anilistId),
	      output::known(e.malId), output::maybe(pageOn(e, site)), output::known(e.kitsuId)})
		cells.push_back(more);
	return cells;
}

// The terminal's colour for each status.
const char* statusColor(watchStatus status) {
	switch (status) {
		case watchStatus::kWatching:
			return "32";
		case watchStatus::kCompleted:
			return "35";
		case watchStatus::kOnHold:
			return "33";
		case watchStatus::kDropped:
			return "31";
		case watchStatus::kPlanToWatch:
			return "90";
	}
	return nullptr;
}

std::string unitName(mediaType type, int count) {
	if (type == mediaType::kAnime)
		return count == 1 ? "episode" : "episodes";
	return count == 1 ? "chapter" : "chapters";
}

// "1 title", "3 titles".
std::string titles(size_t count) {
	return std::format("{} {}", count, count == 1 ? "title" : "titles");
}

// "5/28", or "5/?" while the total is not known.
std::string outOf(int done, int total) {
	return std::format("{}/{}", done, total > 0 ? std::to_string(total) : std::string("?"));
}

// Rows as JSON Lines, or as tab-separated lines (the column names first
// when `header`). A write that fails, because the reader has gone, ends it.
void writeRows(cliContext& ctx, const std::vector<std::string>& names,
               const std::vector<std::vector<output::cell>>& rows, bool json, bool header) {
	if (header and not json)
		ctx.out << output::tsvLine(names);
	for (const auto& row : rows) {
		if (not ctx.out)
			return;
		ctx.out << (json ? output::jsonLine(names, row) : output::tsvLine(row));
	}
}

extern const std::vector<watchStatus> kStatuses = {watchStatus::kWatching, watchStatus::kCompleted,
                                                   watchStatus::kOnHold, watchStatus::kDropped,
                                                   watchStatus::kPlanToWatch};

// The words a status is written with: watching, completed, on-hold, dropped
// and planned.
std::vector<std::string> statusWords() {
	std::vector<std::string> words;
	for (watchStatus status : kStatuses)
		words.push_back(statusKey(status));
	return words;
}

std::optional<std::string> readText(const command& cmd, const args::parsed& line, cliContext& ctx) {
	std::string text;
	for (size_t i = 0; i < line.operands.size(); ++i) {
		const std::string& word = line.operands[i];
		if (i > 0 and word.size() > 1 and word[0] == '-') {
			usageError(ctx, cmd,
			           std::format("'{}' comes after the text; options go before it", word));
			return std::nullopt;
		}
		text += (i > 0 ? " " : "") + word;
	}
	return text;
}

// -a and -m: the one list a command is kept to, or both.
std::optional<mediaType> onlyType(const args::parsed& line) {
	if (line.has("a") == line.has("m"))
		return std::nullopt;
	return line.has("a") ? mediaType::kAnime : mediaType::kManga;
}

// The options of every command that takes REFs.
std::vector<args::option> withRefOptions(std::vector<args::option> options) {
	options.insert(options.begin(), {{'a', "", "", "look in the anime list only"},
	                                 {'m', "", "", "look in the manga list only"},
	                                 {0, "all", "", "take every title a REF matches"}});
	return options;
}

// Reads the REF operands; a `-` among them stands for the lines of standard
// input, which then cannot answer a question as well.
refRequest readRefs(const command& cmd, const args::parsed& line, cliContext& ctx) {
	refRequest request;
	if (line.operands.empty()) {
		request.stop = usageError(ctx, cmd, "name a title: a ref such as a:12, or part of it");
		return request;
	}
	bool fromInput =
	    std::find(line.operands.begin(), line.operands.end(), "-") != line.operands.end();
	request.refs = args::withLinesFor(line.operands, ctx.in);
	request.how = {onlyType(line), line.has("all"), ctx.interactive and not fromInput};
	bool anyEmpty = std::any_of(request.refs.begin(), request.refs.end(),
	                            [](const std::string& ref) { return ref.empty(); });
	if (anyEmpty) {
		request.stop = usageError(ctx, cmd, "a REF cannot be empty");
	} else if (request.refs.empty()) {
		ctx.err << "No titles named on standard input: nothing to do.\n";
		request.stop = 0;
	}
	return request;
}

// Asks which of the numbered choices was meant; nothing when the answer is
// empty or not one of them.
std::optional<size_t> askWhich(cliContext& ctx, size_t count) {
	ctx.err << std::format("Which one? [1-{}, Enter to cancel]: ", count) << std::flush;
	std::string answer;
	if (not std::getline(ctx.in, answer))
		return std::nullopt;
	auto picked = args::parseCount(answer);
	if (not picked or *picked < 1 or static_cast<size_t>(*picked) > count)
		return std::nullopt;
	return static_cast<size_t>(*picked - 1);
}

// Every title the REFs name, each once, in the order named. Nothing when
// any REF fails: a command works on all it was given, or on none.
std::optional<std::vector<entry>> resolve(cliContext& ctx, const std::vector<std::string>& given,
                                          const naming& how) {
	std::vector<entry> all;
	for (const auto& ref : given) {
		auto found = named(ctx, ref, how);
		if (not found)
			return std::nullopt;
		for (const auto& each : *found) {
			bool taken = std::any_of(all.begin(), all.end(), [&each](const entry& other) {
				return other.type == each.type and other.id == each.id;
			});
			if (not taken)
				all.push_back(each);
		}
	}
	return all;
}

bool load(cliContext& ctx) {
	auto loaded = ctx.db.loadAll();
	if (not loaded.has_value())
		fail(ctx, loaded.error());
	return loaded.has_value();
}

// Takes the catalogue for writing, then reads it: nothing another program
// does can come between what the command reads and what it saves.
bool loadForWrite(cliContext& ctx) {
	auto held = ctx.db.beginWrite();
	if (not held.has_value()) {
		fail(ctx, held.error());
		return false;
	}
	return load(ctx);
}

// A line for every field that changed. Into a pipe: the ref, the title, the
// field, its old value and its new one.
void printChanges(cliContext& ctx, const std::vector<journal::change>& changes) {
	for (const auto& made : changes) {
		const entry& shown = made.after ? *made.after : *made.before;
		for (const auto& field : journal::changedFields(made)) {
			if (not ctx.out)
				return;
			ctx.out << (ctx.columns > 0
			                ? changeText(ctx, shown, field)
			                : output::tsvLine({refs::of(shown), shown.title, field.field,
			                                   field.before, field.after}));
		}
	}
}

// Saves what a command changed as one undo step and prints it (not with
// -q). A command that changed nothing says so and saves nothing.
int saveAndReport(cliContext& ctx, const args::parsed& line,
                  const std::vector<journal::change>& changes) {
	bool any = std::any_of(changes.begin(), changes.end(), [](const journal::change& made) {
		return not journal::changedFields(made).empty();
	});
	if (not any) {
		if (not line.has("q"))
			ctx.err << "Nothing changed.\n";
		return 0;
	}
	if (not ctx.db.saveAll(undoNote(ctx)))
		return saveFailed(ctx);
	if (not line.has("q"))
		printChanges(ctx, changes);
	tellSites(ctx, changes);
	return 0;
}

int applyTo(cliContext& ctx, const args::parsed& line, const std::vector<entry>& titles,
            const titleChange& change) {
	std::vector<journal::change> changes;
	for (const auto& before : titles) {
		entry after = change(before);
		ctx.db.updateEntry(after);
		changes.push_back({before.type, before.id, before, after});
	}
	return saveAndReport(ctx, line, changes);
}

// Makes one change to every title the REFs name; every REF is resolved
// before any title changes.
int changeTitles(const command& self, const args::parsed& line, cliContext& ctx,
                 const titleChange& change) {
	auto request = readRefs(self, line, ctx);
	if (request.stop)
		return *request.stop;
	if (not loadForWrite(ctx))
		return 1;
	auto found = resolve(ctx, request.refs, request.how);
	return found ? applyTo(ctx, line, *found, change) : 1;
}

// The N of -n, or `otherwise` when it is not given.
result<int> countGiven(const args::parsed& line, int otherwise) {
	if (not line.has("n"))
		return otherwise;
	auto count = args::parseCount(line.get("n"));
	if (not count or *count == 0)
		return std::unexpected(std::format("-n '{}' is not a count of one or more", line.get("n")));
	return *count;
}

extern const args::option kFrom{
    0, "from", "SITE", "ask anilist, mal or kitsu, not your provider", {"anilist", "mal", "kitsu"}};

bool fromKnown(const command& cmd, const args::parsed& line, cliContext& ctx) {
	if (not line.has("from") or parseProvider(line.get("from")))
		return true;
	usageError(ctx, cmd,
	           std::format("--from '{}' is not anilist, mal or kitsu{}", line.get("from"),
	                       didYouMean(line.get("from"), kFrom.choices)));
	return false;
}

bool asksProvider(const args::parsed& line) {
	return not line.has("from");
}

provider siteOf(const args::parsed& line, const cliContext& ctx) {
	if (auto named = line.has("from") ? parseProvider(line.get("from")) : std::nullopt)
		return *named;
	return ctx.db.getProvider();
}

std::unique_ptr<web::iTracker> trackerFor(const cliContext& ctx, provider site) {
	return web::makeTracker(site, ctx.net);
}

std::vector<account> readAccounts(const cliContext& ctx) {
	return accounts::parse(readFile(ctx.accountsFile).value_or(""));
}

bool saveAccounts(const cliContext& ctx, const std::vector<account>& logins) {
	return writePrivateFile(ctx.accountsFile, accounts::format(logins));
}

std::string webError(const web::failure& why) {
	if (why.why == web::failure::kind::kUnreachable)
		return why.message + "; check the connection and try again";
	return why.message;
}

extern const args::option kQuiet{'q', "", "", "print nothing"};

extern const char* const kRefsHelp =
    "A REF is a ref such as a:12, or part of a title; a REF of - reads REFs\n"
    "from standard input, one a line. Every REF is looked up before anything\n"
    "changes: when one of them fails, nothing is changed.\n";

extern const char* const kChangeFormat =
    "Each field that changed is printed. Into a pipe that is one line of\n"
    "fields with a tab between them: ref, title, field, old value, new value.\n"
    "What a command changed is one step for undo.\n";

}  // namespace catalist::cli
