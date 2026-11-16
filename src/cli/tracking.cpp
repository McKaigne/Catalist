#include <fmt/format.h>

#include <istream>
#include <map>
#include <ostream>

#include "cli/common.hpp"
#include "cli/refs.hpp"
#include "core/text.hpp"

namespace catalist::cli {

namespace {

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

// An empty value clears the date; `today` is today's.
std::optional<entry> withDate(const entry& target, bool start, const std::string& value,
                              const isoDate& today) {
	auto day = value == "today" ? std::optional(today) : isoDate::parse(value);
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
	    {"start", "started", "a date such as 2026-10-20, today, or empty to clear it",
	     [](entry e, text value, day today) { return withDate(e, true, value, today); }},
	    {"finish", "finished", "a date such as 2026-10-20, today, or empty to clear it",
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

// Asks before titles are removed. False, with the reason on standard error,
// when the answer is not yes or when there is nobody to ask.
bool removalConfirmed(cliContext& ctx, const std::vector<entry>& gone, bool ask) {
	for (size_t i = 0; gone.size() > 1 and i < gone.size(); ++i)
		ctx.err << "  " << refs::of(gone[i]) << "  " << gone[i].title << "\n";
	std::string what = gone.size() == 1 ? fmt::format("{} ({})", gone[0].title, refs::of(gone[0]))
	                                    : fmt::format("these {} titles", gone.size());
	if (not ask) {
		fail(ctx, fmt::format("nobody to ask here: pass -f to remove {}", what));
		return false;
	}
	ctx.err << fmt::format("Remove {} from your lists? [y/N]: ", what) << std::flush;
	std::string answer;
	std::getline(ctx.in, answer);
	answer = utf8::lowercase(answer);
	if (answer == "y" or answer == "yes")
		return true;
	ctx.err << "Cancelled.\n";
	return false;
}

int runRemove(const command& self, const args::parsed& line, cliContext& ctx) {
	auto request = readRefs(self, line, ctx);
	if (request.stop)
		return *request.stop;
	if (not loadForWrite(ctx))
		return 1;
	auto found = resolve(ctx, request.refs, request.how);
	if (not found)
		return 1;
	if (not line.has("f") and not removalConfirmed(ctx, *found, request.how.ask))
		return 1;
	std::vector<journal::change> changes;
	for (const auto& gone : *found) {
		ctx.db.removeEntry(gone.type, gone.id);
		changes.push_back({gone.type, gone.id, gone, std::nullopt});
	}
	return saveAndReport(ctx, line, changes);
}

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

}  // namespace

std::vector<std::string> settableNames() {
	std::vector<std::string> names;
	for (const auto& each : settables())
		names.push_back(each.name);
	return names;
}

std::vector<command> trackingCommands() {
	return {
	    stepCommand("+", "inc", "Count one more episode or chapter",
	                "The first one starts a title, and the last one completes it.\n"),
	    stepCommand("-", "dec", "Count one less episode or chapter",
	                "Going back below the total reopens a completed title.\n"),
	    {"set",
	     {},
	     "Set fields of a title",
	     std::string(
	         "The fields: title, status, score (0 clears it), progress, total,\n"
	         "volumes, rewatched, start and finish (dates such as 2026-10-20, or\n"
	         "today; an empty value, as in finish=, clears one). Reaching the total completes a\n"
	         "title, as + does. All values are checked before anything changes.\n\n"
	         "The REF is a ref such as a:12, or part of a title.\n\n") +
	         kChangeFormat,
	     withRefOptions({kQuiet}),
	     "REF FIELD=VALUE...",
	     runSet,
	     operandKind::kRefThenFields},
	    statusCommand("done", "Mark titles completed",
	                  "Sets the finish date, and the progress to the total when it is known.\n"),
	    statusCommand("hold", "Put titles on hold", ""),
	    statusCommand("drop", "Mark titles dropped", ""),
	    statusCommand("plan", "Mark titles planned", ""),
	    statusCommand("resume", "Take titles up again",
	                  "Sets the status to watching (reading, for a manga).\n"),
	    {"rm",
	     {},
	     "Remove titles",
	     std::string("At a terminal rm asks first. Where nobody can be asked, in a script or\n"
	                 "with REFs from standard input, it removes nothing without -f. 'undo'\n"
	                 "brings a removed title back under its ref.\n\n") +
	         kRefsHelp + "\n" + kChangeFormat,
	     withRefOptions({{'f', "", "", "remove without asking"}, kQuiet}),
	     "REF...",
	     runRemove,
	     operandKind::kRefs},
	};
}

}  // namespace catalist::cli
