#include "cli.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <functional>
#include <istream>
#include <map>
#include <optional>
#include <ostream>

#include "cliLegacy.hpp"
#include "output.hpp"
#include "refs.hpp"
#include "text.hpp"
#include "version.hpp"

namespace catalist {

namespace {

using output::paint;

// ---- Messages ----

int fail(cliContext& ctx, const std::string& message) {
	ctx.err << "error: " << message << "\n";
	return 1;
}

int usageError(cliContext& ctx, const command& cmd, const std::string& message) {
	ctx.err << fmt::format("error: {}\nUsage: {} {}\nRun '{} help {}' for more.\n", message,
	                       ctx.program, usageLine(cmd), ctx.program, cmd.name);
	return 2;
}

std::string didYouMean(const std::string& typed, const std::vector<std::string>& known) {
	auto guess = args::closestTo(typed, known);
	return guess ? fmt::format(" (did you mean '{}'?)", *guess) : "";
}

// ---- A title as text ----

// The columns of a title, in the order every tab-separated line and JSON
// object has them. A new column goes at the end: scripts count on the order.
const std::vector<std::string> kColumns = {"ref",      "type",    "title",   "status",    "score",
                                           "progress", "total",   "volumes", "rewatched", "started",
                                           "finished", "updated", "new"};
// What `show` adds to them.
const std::vector<std::string> kShowColumns = {"total_volumes", "released", "anilist_id", "mal_id",
                                               "url"};

std::vector<output::cell> cellsOf(const entry& e) {
	return {output::text(refs::of(e)),     output::text(typeKey(e.type)),
	        output::text(e.title),         output::text(statusKey(e.status)),
	        output::number(e.score),       output::number(e.progress),
	        output::known(e.totalUnits),   output::number(e.volumes),
	        output::number(e.rewatched),   output::date(e.startDate),
	        output::date(e.finishDate),    output::date(e.updatedAt),
	        output::number(newReleases(e))};
}

std::vector<output::cell> showCellsOf(const entry& e, provider site) {
	auto cells = cellsOf(e);
	for (const auto& more :
	     {output::known(e.totalVolumes), output::known(e.released), output::known(e.anilistId),
	      output::known(e.malId), output::maybe(pageOn(e, site))})
		cells.push_back(more);
	return cells;
}

// Close to the tracker's status colours.
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

std::string unitName(mediaType type) {
	return type == mediaType::kAnime ? "episodes" : "chapters";
}

// "5/28", or "5/?" while the total is not known.
std::string outOf(int done, int total) {
	return fmt::format("{}/{}", done, total > 0 ? std::to_string(total) : std::string("?"));
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

// ---- Naming titles ----

// -a and -m: the one list a command is kept to, or both.
std::optional<mediaType> onlyType(const args::parsed& line) {
	if (line.has("a") == line.has("m"))
		return std::nullopt;
	return line.has("a") ? mediaType::kAnime : mediaType::kManga;
}

// How the REFs of a command are matched: in which list, whether a REF may
// stand for several titles, and whether the person can be asked which.
struct naming {
	std::optional<mediaType> only;
	bool all = false;
	bool ask = false;
};

// The options of every command that takes REFs.
std::vector<args::option> withRefOptions(std::vector<args::option> options) {
	options.insert(options.begin(), {{'a', "", "", "look in the anime list only"},
	                                 {'m', "", "", "look in the manga list only"},
	                                 {0, "all", "", "take every title a REF matches"}});
	return options;
}

// The REFs given, or the exit code when there is nothing to go on with.
struct refRequest {
	std::vector<std::string> refs;
	naming how;
	std::optional<int> stop;
};

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
	ctx.err << fmt::format("Which one? [1-{}, Enter to cancel]: ", count) << std::flush;
	std::string answer;
	if (not std::getline(ctx.in, answer))
		return std::nullopt;
	auto picked = args::parseCount(answer);
	if (not picked or *picked < 1 or static_cast<size_t>(*picked) > count)
		return std::nullopt;
	return static_cast<size_t>(*picked - 1);
}

void sayNoMatch(cliContext& ctx, const std::string& ref, const naming& how) {
	std::string where = how.only ? fmt::format("your {} list", typeKey(*how.only)) : "your lists";
	fail(ctx, refs::parse(ref) ? fmt::format("no title in {} has the ref {}", where, ref)
	                           : fmt::format("nothing in {} matches '{}'", where, ref));
	if (ref.size() > 1 and ref[0] == '-')
		ctx.err << "Options go before the first operand.\n";
	ctx.err << fmt::format("See what is there with: {} ls\n", ctx.program);
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

	ctx.err << fmt::format("{}'{}' matches {} titles:\n", how.ask ? "" : "error: ", ref,
	                       found.size());
	for (size_t i = 0; i < found.size(); ++i)
		ctx.err << (how.ask ? fmt::format("  {:>2}", i + 1) : "") << "  " << refs::of(found[i])
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
	if (not loaded.ok())
		fail(ctx, loaded.errorMessage());
	ctx.err << importNote(ctx.db);
	return loaded.ok();
}

// ---- Changing titles ----

// Takes the catalogue for writing, then reads it: nothing another program
// does can come between what the command reads and what it saves.
bool loadForWrite(cliContext& ctx) {
	auto held = ctx.db.beginWrite();
	if (not held.ok()) {
		fail(ctx, held.errorMessage());
		return false;
	}
	return load(ctx);
}

// One changed field as a terminal shows it: "Odd Taxi  5 → 6/13" for the
// progress, "Odd Taxi  score 8 → 9" for the rest.
std::string changeText(const cliContext& ctx, const entry& shown,
                       const journal::fieldChange& field) {
	std::string title = paint(shown.title, output::kBold, ctx.color);
	if (field.field == "added")
		return fmt::format("{}  added as {}\n", title, field.after);
	if (field.field == "removed")
		return fmt::format("{}  removed\n", title);
	if (field.field == "progress")
		return fmt::format("{}  {} → {}\n", title, field.before,
		                   outOf(shown.progress, shown.totalUnits));
	return fmt::format("{}  {} {} → {}\n", title, paint(field.field, output::kDim, ctx.color),
	                   field.before, field.after);
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
		ctx.err << "Nothing changed.\n";
		return 0;
	}
	if (not ctx.db.saveAll(undoNote(ctx)))
		return fail(
		    ctx, fmt::format("could not save the catalogue to {}", ctx.db.getDataDir().string()));
	if (not line.has("q"))
		printChanges(ctx, changes);
	return 0;
}

using titleChange = std::function<entry(const entry&)>;

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
		return result<int>::success(otherwise);
	auto count = args::parseCount(line.get("n"));
	if (not count or *count == 0)
		return result<int>::failure(
		    fmt::format("-n '{}' is not a count of one or more", line.get("n")));
	return result<int>::success(*count);
}

const args::option kQuiet{'q', "", "", "print nothing"};

// ---- The fields of `set` ----

// A field `set` can set: its name, another name it goes by, what its value
// must be, and the rule that sets it (nothing when the value is not one).
struct settable {
	std::string name;
	std::string alias;
	std::string wants;
	std::optional<entry> (*set)(entry target, const std::string& value, const isoDate& today);
};

std::optional<entry> withCount(const entry& target, countField field, const std::string& value,
                               const isoDate& today) {
	auto count = args::parseCount(value);
	return count ? std::optional(setCount(target, field, *count, today)) : std::nullopt;
}

// An empty value clears the date.
std::optional<entry> withDate(const entry& target, bool start, const std::string& value,
                              const isoDate& today) {
	auto day = isoDate::parse(value);
	if (not day and not value.empty())
		return std::nullopt;
	return start ? setStartDate(target, day, today) : setFinishDate(target, day, today);
}

// In the order they are set when several are given: the total first, so the
// progress can reach it; the status after the progress, so a status that is
// given stands (completed still means all of a known total); the dates last,
// so they stand whatever the status did.
const std::vector<settable>& settables() {
	using text = const std::string&;
	using day = const isoDate&;
	static const std::vector<settable> all = {
	    {"total", "", "a count",
	     [](entry e, text value, day today) {
		     return withCount(e, countField::kTotalUnits, value, today);
	     }},
	    {"progress", "", "a count",
	     [](entry e, text value, day today) {
		     return withCount(e, countField::kProgress, value, today);
	     }},
	    {"status", "", "watching, completed, on-hold, dropped or planned",
	     [](entry e, text value, day today) {
		     auto status = parseStatus(value);
		     return status ? std::optional(setStatus(e, *status, today)) : std::nullopt;
	     }},
	    {"score", "", "0 to 10",
	     [](entry e, text value, day today) {
		     auto score = args::parseCount(value);
		     bool valid = score and *score <= 10;
		     return valid ? std::optional(setScore(e, *score, today)) : std::nullopt;
	     }},
	    {"volumes", "", "a count",
	     [](entry e, text value, day today) {
		     return withCount(e, countField::kVolumes, value, today);
	     }},
	    {"rewatched", "", "a count",
	     [](entry e, text value, day today) {
		     return withCount(e, countField::kRewatched, value, today);
	     }},
	    {"title", "", "a title",
	     [](entry e, text value, day today) {
		     return value.empty() ? std::nullopt : std::optional(setTitle(e, value, today));
	     }},
	    {"start", "started", "a date such as 2026-10-20, or empty to clear it",
	     [](entry e, text value, day today) { return withDate(e, true, value, today); }},
	    {"finish", "finished", "a date such as 2026-10-20, or empty to clear it",
	     [](entry e, text value, day today) { return withDate(e, false, value, today); }},
	};
	return all;
}

const settable* findSettable(const std::string& name) {
	for (const auto& each : settables())
		if (each.name == name or (not each.alias.empty() and each.alias == name))
			return &each;
	return nullptr;
}

std::vector<std::string> settableNames() {
	std::vector<std::string> names;
	for (const auto& each : settables())
		names.push_back(each.name);
	return names;
}

// The FIELD=VALUE operands (all but the first, the REF) by field name;
// fails on the first that names no field or holds no value for it.
result<std::map<std::string, std::string>> readAssignments(const std::vector<std::string>& operands,
                                                           const isoDate& today) {
	using outcome = result<std::map<std::string, std::string>>;
	std::map<std::string, std::string> values;
	for (size_t i = 1; i < operands.size(); ++i) {
		auto equals = operands[i].find('=');
		if (equals == std::string::npos)
			return outcome::failure(fmt::format(
			    "'{}' is not FIELD=VALUE; quote a title of several words", operands[i]));
		std::string name = utf8::lowercase(operands[i].substr(0, equals));
		std::string value = operands[i].substr(equals + 1);
		const settable* field = findSettable(name);
		if (not field)
			return outcome::failure(
			    fmt::format("unknown field '{}'{}", name, didYouMean(name, settableNames())));
		if (not field->set(entry{}, value, today))
			return outcome::failure(
			    fmt::format("{} '{}' is not {}", field->name, value, field->wants));
		values[field->name] = value;
	}
	return outcome::success(std::move(values));
}

entry withValues(entry target, const std::map<std::string, std::string>& values,
                 const isoDate& today) {
	for (const auto& field : settables()) {
		auto given = values.find(field.name);
		if (given != values.end())
			target = *field.set(target, given->second, today);
	}
	return target;
}

// ---- Help ----

const command* findCommand(const std::string& name) {
	for (const auto& each : commands()) {
		if (each.name == name or
		    std::find(each.aliases.begin(), each.aliases.end(), name) != each.aliases.end())
			return &each;
	}
	return nullptr;
}

// What a mistyped command may have been meant to be.
std::vector<std::string> commandNames() {
	std::vector<std::string> names;
	for (const auto& each : commands()) {
		if (each.hidden)
			continue;
		names.push_back(each.name);
		names.insert(names.end(), each.aliases.begin(), each.aliases.end());
	}
	for (const auto& old : legacySummaries())
		names.push_back(old.first);
	return names;
}

// "-s STATUS", "    --sort=KEY", "-h, --help".
std::string optionLabel(const args::option& shown) {
	std::string label = shown.letter ? fmt::format("-{}", shown.letter) : "  ";
	if (not shown.name.empty())
		label += fmt::format("{} --{}", shown.letter ? "," : " ", shown.name);
	if (not shown.value.empty())
		label += fmt::format("{}{}", shown.name.empty() ? " " : "=", shown.value);
	return label;
}

void printCommandHelp(cliContext& ctx, const command& cmd) {
	ctx.out << fmt::format("{}.\n\nUsage: {} {}\n", cmd.summary, ctx.program, usageLine(cmd));
	if (not cmd.aliases.empty()) {
		std::string also;
		for (const auto& alias : cmd.aliases)
			also += fmt::format("{}{} {}", also.empty() ? "" : ", ", ctx.program, alias);
		ctx.out << fmt::format("Also:  {}\n", also);
	}
	if (not cmd.details.empty())
		ctx.out << "\n" << cmd.details;
	ctx.out << "\nOptions:\n";
	std::vector<args::option> options = cmd.options;
	options.push_back(helpOption());
	for (const auto& each : options)
		ctx.out << fmt::format("  {:<18}{}\n", optionLabel(each), each.help);
}

void printHelp(cliContext& ctx) {
	const std::string& program = ctx.program;
	ctx.out << fmt::format(
	    "Catalist {} - track anime and manga from the terminal.\n\n"
	    "Usage: {} [COMMAND] [ARG...]\n\n"
	    "Run without a command to open the tracker.\n\nCommands:\n",
	    kVersionString, program);
	for (const auto& each : commands())
		if (not each.hidden)
			ctx.out << fmt::format("  {:<12}{}\n", each.name, each.summary);
	for (const auto& old : legacySummaries())
		ctx.out << fmt::format("  {:<12}{}\n", old.first, old.second);
	ctx.out << fmt::format(
	    "\nName a title by its ref, the first column of '{0} ls' (a:12 is an anime,\n"
	    "m:3 a manga), or by part of its title:\n"
	    "  {0} show a:12\n"
	    "  {0} + frieren\n"
	    "  {0} set a:12 score=9\n"
	    "  {0} undo\n"
	    "\nOptions come before the titles. Run '{0} help COMMAND' for the options of\n"
	    "a command. Your lists are kept in\n{1}.\n",
	    program, ctx.db.getDataDir().string());
}

// ---- Commands ----

// The list as a table for a terminal.
std::string listTable(const cliContext& ctx, const std::vector<entry>& shown) {
	std::vector<output::tableColumn> columns = {{"REF"},         {"TITLE", false, true}, {"STATUS"},
	                                            {"SCORE", true}, {"PROGRESS"},           {"NEW"}};
	std::vector<std::vector<output::styled>> rows;
	for (const auto& e : shown) {
		int fresh = newReleases(e);
		rows.push_back({{refs::of(e), output::kCyan},
		                {e.title},
		                {statusToString(e.status, e.type), statusColor(e.status)},
		                {e.score > 0 ? std::to_string(e.score) : "-"},
		                {outOf(e.progress, e.totalUnits)},
		                {fresh > 0 ? fmt::format("+{}", fresh) : "", output::kGreen}});
	}
	return output::table(columns, rows, ctx.columns, ctx.color);
}

const std::map<std::string, sortKey> kSortKeys = {
    {"new", sortKey::kNew},       {"updated", sortKey::kUpdated}, {"score", sortKey::kScore},
    {"status", sortKey::kStatus}, {"title", sortKey::kTitle},     {"progress", sortKey::kProgress},
    {"date", sortKey::kDate}};

int runList(const command& self, const args::parsed& line, cliContext& ctx) {
	if (line.operands.size() > 1)
		return usageError(ctx, self,
		                  "give one QUERY, after the options; quote a title of several words");
	std::optional<watchStatus> status;
	if (line.has("s")) {
		status = parseStatus(line.get("s"));
		if (not status)
			return usageError(ctx, self, fmt::format("unknown status '{}'", line.get("s")));
	}
	sortKey key = sortKey::kTitle;
	if (line.has("sort")) {
		auto found = kSortKeys.find(line.get("sort"));
		if (found == kSortKeys.end())
			return usageError(ctx, self, fmt::format("unknown sort key '{}'", line.get("sort")));
		key = found->second;
	}
	if (not load(ctx))
		return 1;

	std::string query = line.operands.empty() ? "" : line.operands[0];
	auto only = onlyType(line);
	std::vector<entry> shown;
	for (mediaType type : {mediaType::kAnime, mediaType::kManga}) {
		if (only and *only != type)
			continue;
		for (const auto& e : ctx.db.getEntries(type))
			if ((not status or e.status == *status) and refs::titleHolds(e, query))
				shown.push_back(e);
	}
	catalogue::sortEntries(shown, key, line.has("r"));

	if (line.has("q")) {
		for (const auto& e : shown)
			ctx.out << refs::of(e) << "\n";
		return 0;
	}
	if (not line.has("json") and ctx.columns > 0) {
		if (shown.empty())
			ctx.err << "No titles to list.\n";
		else
			ctx.out << listTable(ctx, shown);
		return 0;
	}
	std::vector<std::vector<output::cell>> rows;
	for (const auto& e : shown)
		rows.push_back(cellsOf(e));
	writeRows(ctx, kColumns, rows, line.has("json"), line.has("header"));
	return 0;
}

// Everything about one title, for a terminal.
void printCard(cliContext& ctx, const entry& e) {
	auto row = [&ctx](const std::string& label, const std::string& value) {
		ctx.out << "  " << paint(fmt::format("{:<13}", label), output::kDim, ctx.color) << value
		        << "\n";
	};
	auto idText = [](int id) { return id > 0 ? std::to_string(id) : std::string("-"); };
	auto dateText = [](const std::optional<isoDate>& day) {
		return day ? day->toString() : std::string("-");
	};
	std::string page = pageOn(e, ctx.db.getProvider());
	int fresh = newReleases(e);

	ctx.out << paint(e.title, output::kBold, ctx.color) << "\n";
	row("Ref", paint(refs::of(e), output::kCyan, ctx.color));
	row("Type", typeToString(e.type));
	row("AniList", idText(e.anilistId));
	row("MyAnimeList", idText(e.malId));
	if (not page.empty())
		row("Page", page);
	row("Status", paint(statusToString(e.status, e.type), statusColor(e.status), ctx.color));
	row("Score", e.score > 0 ? fmt::format("{}/10", e.score) : "-");
	row("Progress",
	    fmt::format("{} {}", outOf(e.progress, e.totalUnits), unitName(e.type)) +
	        (fresh > 0 ? paint(fmt::format("  (+{} new)", fresh), output::kGreen, ctx.color) : ""));
	if (e.type == mediaType::kManga)
		row("Volumes", outOf(e.volumes, e.totalVolumes));
	row(e.type == mediaType::kAnime ? "Rewatched" : "Reread", std::to_string(e.rewatched));
	row("Started", dateText(e.startDate));
	row("Finished", dateText(e.finishDate));
	row("Updated", dateText(e.updatedAt));
}

int runShow(const command& self, const args::parsed& line, cliContext& ctx) {
	auto request = readRefs(self, line, ctx);
	if (request.stop)
		return *request.stop;
	if (not load(ctx))
		return 1;
	auto found = resolve(ctx, request.refs, request.how);
	if (not found)
		return 1;

	if (not line.has("json") and ctx.columns > 0) {
		for (size_t i = 0; i < found->size() and ctx.out; ++i) {
			ctx.out << (i > 0 ? "\n" : "");
			printCard(ctx, (*found)[i]);
		}
		return 0;
	}
	std::vector<std::string> names = kColumns;
	names.insert(names.end(), kShowColumns.begin(), kShowColumns.end());
	std::vector<std::vector<output::cell>> rows;
	for (const auto& e : *found)
		rows.push_back(showCellsOf(e, ctx.db.getProvider()));
	writeRows(ctx, names, rows, line.has("json"), line.has("header"));
	return 0;
}

int runStep(const command& self, const args::parsed& line, cliContext& ctx) {
	auto by = countGiven(line, 1);
	if (not by.ok())
		return usageError(ctx, self, by.errorMessage());
	int delta = self.name == "+" ? by.value() : -by.value();
	isoDate today = ctx.today();
	return changeTitles(self, line, ctx,
	                    [&](const entry& e) { return stepProgress(e, delta, today); });
}

int runSet(const command& self, const args::parsed& line, cliContext& ctx) {
	if (line.operands.empty() or line.operands[0].empty())
		return usageError(ctx, self, "name a title: a ref such as a:12, or part of it");
	if (line.operands.size() < 2)
		return usageError(ctx, self, "nothing to change: give a FIELD=VALUE such as score=8");
	isoDate today = ctx.today();
	auto values = readAssignments(line.operands, today);
	if (not values.ok())
		return usageError(ctx, self, values.errorMessage());
	if (not loadForWrite(ctx))
		return 1;
	auto found =
	    resolve(ctx, {line.operands[0]}, {onlyType(line), line.has("all"), ctx.interactive});
	if (not found)
		return 1;
	return applyTo(ctx, line, *found,
	               [&](const entry& e) { return withValues(e, values.value(), today); });
}

// The status each of the status commands sets.
const std::map<std::string, watchStatus> kStatusCommands = {{"done", watchStatus::kCompleted},
                                                            {"hold", watchStatus::kOnHold},
                                                            {"drop", watchStatus::kDropped},
                                                            {"plan", watchStatus::kPlanToWatch},
                                                            {"resume", watchStatus::kWatching}};

int runStatus(const command& self, const args::parsed& line, cliContext& ctx) {
	watchStatus status = kStatusCommands.at(self.name);
	std::optional<int> score;
	if (line.has("score")) {
		score = args::parseCount(line.get("score"));
		if (not score or *score > 10)
			return usageError(ctx, self,
			                  fmt::format("--score '{}' is not 0 to 10", line.get("score")));
	}
	isoDate today = ctx.today();
	return changeTitles(self, line, ctx, [&](const entry& e) {
		entry next = setStatus(e, status, today);
		return score ? setScore(next, *score, today) : next;
	});
}

// undo and redo.
int runReplay(const command& self, const args::parsed& line, cliContext& ctx) {
	bool redoing = self.name == "redo";
	if (not line.operands.empty())
		return usageError(ctx, self, fmt::format("{} takes no operands", self.name));
	auto count = countGiven(line, 1);
	if (not count.ok())
		return usageError(ctx, self, count.errorMessage());
	if (not loadForWrite(ctx))
		return 1;

	bool force = line.has("f");
	auto done = redoing ? ctx.db.redo(count.value(), force) : ctx.db.undo(count.value(), force);
	if (not done.ok()) {
		fail(ctx, done.errorMessage());
		if (not force)
			ctx.err << "Nothing was changed. With -f the step is taken all the same, where "
			           "it still can be.\n";
		return 1;
	}
	if (done.value().steps.empty()) {
		ctx.err << fmt::format("Nothing to {}.\n", self.name);
		return 0;
	}
	if (line.has("q"))
		return 0;
	for (const auto& step : done.value().steps)
		ctx.err << fmt::format("{} '{}' ({}).\n", redoing ? "Redid" : "Undid", step.label,
		                       step.stamp);
	printChanges(ctx, done.value().changes);
	return 0;
}

int runLog(const command& self, const args::parsed& line, cliContext& ctx) {
	if (not line.operands.empty())
		return usageError(ctx, self, "log takes no operands");
	auto count = countGiven(line, 0);
	if (not count.ok())
		return usageError(ctx, self, count.errorMessage());
	if (not load(ctx))
		return 1;
	auto steps = ctx.db.history(count.value());
	if (not steps.ok())
		return fail(ctx, steps.errorMessage());

	auto stateOf = [](const txRecord& step) { return step.undone ? "undone" : "done"; };
	if (not line.has("json") and ctx.columns > 0) {
		std::vector<std::vector<output::styled>> rows;
		for (const auto& step : steps.value())
			rows.push_back({{std::to_string(step.id), output::kCyan},
			                {step.stamp},
			                {stateOf(step), step.undone ? output::kDim : nullptr},
			                {std::to_string(step.changes)},
			                {step.label}});
		if (rows.empty())
			ctx.err << "Nothing has been changed yet.\n";
		else
			ctx.out << output::table(
			    {{"TX", true}, {"WHEN"}, {"STATE"}, {"CHANGES", true}, {"COMMAND", false, true}},
			    rows, ctx.columns, ctx.color);
		return 0;
	}
	std::vector<std::vector<output::cell>> rows;
	for (const auto& step : steps.value())
		rows.push_back({output::number(step.id), output::text(step.stamp),
		                output::text(stateOf(step)), output::number(step.changes),
		                output::text(step.label)});
	writeRows(ctx, {"tx", "stamp", "state", "changes", "label"}, rows, line.has("json"),
	          line.has("header"));
	return 0;
}

int runTx(const command& self, const args::parsed& line, cliContext& ctx) {
	if (not line.operands.empty())
		return usageError(ctx, self, "tx takes no operands");
	ctx.out << ctx.newTxLabel() << "\n";
	return 0;
}

int runHelp(const command& self, const args::parsed& line, cliContext& ctx) {
	if (line.operands.empty()) {
		printHelp(ctx);
		return 0;
	}
	if (line.operands.size() > 1)
		return usageError(ctx, self, "name one command");
	const std::string& name = line.operands[0];
	if (const command* asked = findCommand(name)) {
		printCommandHelp(ctx, *asked);
		return 0;
	}
	if (printLegacyHelp(name, ctx.out))
		return 0;
	return usageError(ctx, self,
	                  fmt::format("no command '{}'{}", name, didYouMean(name, commandNames())));
}

const char* const kRowFormat =
    "At a terminal the titles are drawn as a table. Into a pipe or a file each\n"
    "title is one line of fields with a tab between them, so cut, awk and\n"
    "read can take them apart:\n"
    "  ref type title status score progress total volumes rewatched\n"
    "  started finished updated new\n"
    "A date that is not set is '-' there, and a total that is not known is 0.\n";

const char* const kRefsHelp =
    "A REF is a ref such as a:12, or part of a title; a REF of - reads REFs\n"
    "from standard input, one a line. Every REF is looked up before anything\n"
    "changes: when one of them fails, nothing is changed.\n";

const char* const kChangeFormat =
    "Each field that changed is printed. Into a pipe that is one line of\n"
    "fields with a tab between them: ref, title, field, old value, new value.\n"
    "What a command changed is one step for undo.\n";

// done, hold, drop, plan and resume differ in their name and their words.
command statusCommand(const std::string& name, const std::string& summary,
                      const std::string& details) {
	std::vector<args::option> options = {kQuiet};
	if (name == "done")
		options.insert(options.begin(), {0, "score", "N", "give it a score as well, 0 to 10"});
	return {name,
	        {},
	        summary,
	        details + "\n" + kRefsHelp + "\n" + kChangeFormat,
	        withRefOptions(options),
	        "REF...",
	        runStatus,
	        operandKind::kRefs};
}

command stepCommand(const std::string& name, const std::string& alias, const std::string& summary,
                    const std::string& details) {
	return {name,
	        {alias},
	        summary,
	        details + "\n" + kRefsHelp + "\n" + kChangeFormat,
	        withRefOptions({{'n', "", "N", "count N instead of one"}, kQuiet}),
	        "REF...",
	        runStep,
	        operandKind::kRefs};
}

command replayCommand(const std::string& name, const std::string& summary,
                      const std::string& details) {
	return {name,
	        {},
	        summary,
	        details,
	        {{'n', "", "N", "N steps instead of one"},
	         {'f', "", "", "also when a title has changed since"},
	         kQuiet},
	        "",
	        runReplay};
}

int unknownCommand(cliContext& ctx, const std::string& first) {
	std::string what =
	    first.rfind('-', 0) == 0 and first.size() > 1
	        ? fmt::format("unknown option '{}'", first)
	        : fmt::format("unknown command '{}'{}", first, didYouMean(first, commandNames()));
	ctx.err << fmt::format(
	    "error: {}\nUsage: {} [COMMAND] [ARG...]\nRun '{} help' to see the commands.\n", what,
	    ctx.program, ctx.program);
	return 2;
}

}  // namespace

const std::vector<command>& commands() {
	static const std::vector<command> all = {
	    {"ls",
	     {},
	     "List your titles",
	     std::string("Lists both lists in title order. QUERY keeps the titles that hold it,\n"
	                 "in capitals or not.\n\n") +
	         kRowFormat,
	     {{'a', "", "", "list anime only"},
	      {'m', "", "", "list manga only"},
	      {'s', "", "STATUS", "only watching, completed, on-hold, dropped or planned"},
	      {0, "sort", "KEY", "new, updated, score, progress, date, status or title"},
	      {'r', "", "", "reverse the order"},
	      {'q', "", "", "print the refs and nothing else"},
	      {0, "json", "", "print one JSON object a line"},
	      {0, "header", "", "name the columns on the first line of a pipe"}},
	     "[QUERY]",
	     runList,
	     operandKind::kQuery},
	    {"show",
	     {},
	     "Show titles in full",
	     "A REF is a ref such as a:12, or part of a title; a REF of - reads REFs\n"
	     "from standard input, one a line. Into a pipe a title is one line: the\n"
	     "columns of ls, then total_volumes, released, anilist_id, mal_id and url.\n",
	     withRefOptions({{0, "json", "", "print one JSON object a line"},
	                     {0, "header", "", "name the columns on the first line of a pipe"}}),
	     "REF...",
	     runShow,
	     operandKind::kRefs},
	    stepCommand("+", "inc", "Count one more episode or chapter",
	                "The first one starts a title, and the last one completes it, as in the\n"
	                "tracker.\n"),
	    stepCommand("-", "dec", "Count one less episode or chapter",
	                "Going back below the total reopens a completed title.\n"),
	    {"set",
	     {},
	     "Set fields of a title",
	     std::string("The fields: title, status, score (0 clears it), progress, total,\n"
	                 "volumes, rewatched, start and finish (dates such as 2026-10-20; an\n"
	                 "empty value, as in finish=, clears one). Progress and status follow the\n"
	                 "rules of the tracker: reaching the total completes a title. All values\n"
	                 "are checked before anything changes.\n\n"
	                 "The REF is a ref such as a:12, or part of a title.\n\n") +
	         kChangeFormat,
	     withRefOptions({kQuiet}),
	     "REF FIELD=VALUE...",
	     runSet,
	     operandKind::kRefs},
	    statusCommand("done", "Mark titles completed",
	                  "Sets the finish date, and the progress to the total when it is known.\n"),
	    statusCommand("hold", "Put titles on hold", ""),
	    statusCommand("drop", "Mark titles dropped", ""),
	    statusCommand("plan", "Mark titles planned", ""),
	    statusCommand("resume", "Take titles up again",
	                  "Sets the status to watching (reading, for a manga).\n"),
	    replayCommand("undo", "Take back what the last command changed",
	                  "Takes back the newest step of 'log' that is not undone: what one\n"
	                  "command changed, or a group of commands run under one CATALIST_TX (see\n"
	                  "'help tx'). A removed title comes back under its old ref. When a\n"
	                  "title is no longer as the step left it, nothing is taken back and the\n"
	                  "title is named; -f takes the step back all the same.\n"),
	    replayCommand("redo", "Make again what undo took back",
	                  "Steps can be made again until a command changes something new.\n"),
	    {"log",
	     {},
	     "List what can be undone",
	     "The last 100 steps, newest first. Into a pipe each is one line of fields\n"
	     "with a tab between them: tx, stamp, state (done or undone), changes (how\n"
	     "many titles) and label (the command).\n",
	     {{'n', "", "N", "the newest N steps only"},
	      {0, "json", "", "print one JSON object a line"},
	      {0, "header", "", "name the columns on the first line of a pipe"}},
	     "",
	     runLog},
	    {"tx",
	     {},
	     "Print a label that groups commands for undo",
	     "Commands run with the same CATALIST_TX in their environment are one step\n"
	     "for undo. In sh, bash or zsh:\n"
	     "  export CATALIST_TX=$(catalist tx)\n"
	     "  catalist ls -q -s watching | catalist + -\n"
	     "  catalist done a:12\n"
	     "  unset CATALIST_TX\n"
	     "In fish:\n"
	     "  set -lx CATALIST_TX (catalist tx)\n"
	     "  ...\n"
	     "  set -e CATALIST_TX\n",
	     {},
	     "",
	     runTx},
	    {"help",
	     {},
	     "Show help for a command",
	     "",
	     {},
	     "[COMMAND]",
	     runHelp,
	     operandKind::kCommands},
	};
	return all;
}

const args::option& helpOption() {
	static const args::option help{'h', "help", "", "show this help"};
	return help;
}

std::string usageLine(const command& shown) {
	std::string flags;
	std::string valued;
	for (const auto& each : shown.options) {
		if (each.value.empty() and each.letter)
			flags += each.letter;
		else if (each.value.empty())
			valued += fmt::format(" [--{}]", each.name);
		else if (each.letter)
			valued += fmt::format(" [-{} {}]", each.letter, each.value);
		else
			valued += fmt::format(" [--{}={}]", each.name, each.value);
	}
	return fmt::format("{}{}{}{}", shown.name, flags.empty() ? "" : fmt::format(" [-{}]", flags),
	                   valued, shown.operands.empty() ? "" : fmt::format(" {}", shown.operands));
}

std::string importNote(const catalogue& db) {
	if (db.importedFrom().empty())
		return "";
	return fmt::format(
	    "note: imported the lists in {} into {}; the old files are left as "
	    "they were.\n",
	    db.importedFrom().string(), db.getStorePath().string());
}

txNote undoNote(const cliContext& ctx) {
	if (not ctx.txLabel.empty())
		return {ctx.txLabel, ctx.nowStamp(), true};
	return {utf8::clip(ctx.typed, 60), ctx.nowStamp(), false};
}

int runCli(const std::vector<std::string>& args, cliContext& ctx) {
	const std::string& first = args.at(0);
	ctx.typed.clear();
	for (const auto& word : args)
		ctx.typed += (ctx.typed.empty() ? "" : " ") + word;
	if (first == "--version" or first == "-V" or first == "-v") {
		ctx.out << "Catalist v" << kVersionString << "\n";
		return 0;
	}
	if (first == "--help" or first == "-h") {
		printHelp(ctx);
		return 0;
	}
	const command* cmd = findCommand(first);
	if (not cmd) {
		auto old = runLegacyCli(args, ctx);
		return old ? *old : unknownCommand(ctx, first);
	}
	std::vector<args::option> options = cmd->options;
	options.push_back(helpOption());
	auto line = args::parse({args.begin() + 1, args.end()}, options);
	if (not line.ok())
		return usageError(ctx, *cmd, line.errorMessage());
	if (line.value().has("help")) {
		printCommandHelp(ctx, *cmd);
		return 0;
	}
	return cmd->run(*cmd, line.value(), ctx);
}

}  // namespace catalist
