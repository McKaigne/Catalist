#include <algorithm>
#include <cctype>
#include <format>
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
struct Settable {
	std::string name;
	std::string alias;
	std::string wants;
	std::optional<Entry> (*set)(Entry target, const std::string& value, const IsoDate& today);
};

std::optional<Entry> WithCount(const Entry& target, CountField field, const std::string& value,
                               const IsoDate& today) {
	auto count = args::ParseCount(value);
	return count ? std::optional(SetCount(target, field, *count, today)) : std::nullopt;
}

// An empty value clears the date; `today` is today's.
std::optional<Entry> WithDate(const Entry& target, bool start, const std::string& value,
                              const IsoDate& today) {
	auto day = value == "today" ? std::optional(today) : IsoDate::Parse(value);
	if (not day and not value.empty())
		return std::nullopt;
	return start ? SetStartDate(target, day, today) : SetFinishDate(target, day, today);
}

// In the order they are set when several are given: the total first, so the
// progress can reach it; the status after the progress, so a status that is
// given stands (completed still means all of a known total); the dates last,
// so they stand whatever the status did.
const std::vector<Settable>& Settables() {
	using Text = const std::string&;
	using Day = const IsoDate&;
	static const std::vector<Settable> kAll = {
	    {"total", "", "a count",
	     [](Entry e, Text value, Day today) {
		     return WithCount(e, CountField::kTotalUnits, value, today);
	     }},
	    {"progress", "", "a count",
	     [](Entry e, Text value, Day today) {
		     return WithCount(e, CountField::kProgress, value, today);
	     }},
	    {"status", "", "watching, completed, on-hold, dropped, planned or rewatching",
	     [](Entry e, Text value, Day today) {
		     auto status = StatusNamed(value);
		     return status ? std::optional(SetStatus(e, *status, today)) : std::nullopt;
	     }},
	    {"score", "", "0 to 10",
	     [](Entry e, Text value, Day today) {
		     auto score = args::ParseCount(value);
		     bool valid = score and *score <= 10;
		     return valid ? std::optional(SetScore(e, *score, today)) : std::nullopt;
	     }},
	    {"volumes", "", "a count",
	     [](Entry e, Text value, Day today) {
		     return WithCount(e, CountField::kVolumes, value, today);
	     }},
	    {"rewatched", "", "a count",
	     [](Entry e, Text value, Day today) {
		     return WithCount(e, CountField::kRewatched, value, today);
	     }},
	    {"title", "", "a title",
	     [](Entry e, Text value, Day today) {
		     return value.empty() ? std::nullopt : std::optional(SetTitle(e, value, today));
	     }},
	    {"start", "started", "a date such as 2026-10-20, today, or empty to clear it",
	     [](Entry e, Text value, Day today) { return WithDate(e, true, value, today); }},
	    {"finish", "finished", "a date such as 2026-10-20, today, or empty to clear it",
	     [](Entry e, Text value, Day today) { return WithDate(e, false, value, today); }},
	};
	return kAll;
}

const Settable* FindSettable(const std::string& name) {
	for (const auto& each : Settables())
		if (each.name == name or (not each.alias.empty() and each.alias == name))
			return &each;
	return nullptr;
}

// A word such as score=9; a title can hold an = too, but not after letters
// alone.
bool IsAssignment(const std::string& word) {
	auto equals = word.find('=');
	return equals != std::string::npos and equals > 0 and
	       std::all_of(word.begin(), word.begin() + static_cast<std::ptrdiff_t>(equals),
	                   [](unsigned char c) { return std::isalpha(c); });
}

// The FIELD=VALUE words by field name; fails on the first that names no field
// or holds no value for it.
Result<std::map<std::string, std::string>> ReadAssignments(const std::vector<std::string>& words,
                                                           const IsoDate& today) {
	std::map<std::string, std::string> values;
	for (const auto& word : words) {
		if (not IsAssignment(word))
			return std::unexpected(std::format("'{}' is not FIELD=VALUE", word));
		auto equals = word.find('=');
		std::string name = utf8::Lowercase(word.substr(0, equals));
		std::string value = word.substr(equals + 1);
		const Settable* field = FindSettable(name);
		if (not field)
			return std::unexpected(std::format("unknown field '{}'", name));
		if (not field->set(Entry{}, value, today))
			return std::unexpected(
			    std::format("{} '{}' is not {}", field->name, value, field->wants));
		values[field->name] = value;
	}
	return values;
}

// The status is judged as it will be reached: after the fields set before it.
std::optional<std::string> RefusedValues(Entry target,
                                         const std::map<std::string, std::string>& values,
                                         const IsoDate& today) {
	for (const auto& field : Settables()) {
		auto given = values.find(field.name);
		if (given == values.end())
			continue;
		if (field.name == "status")
			return StatusRefusal(target, *StatusNamed(given->second));
		target = *field.set(target, given->second, today);
	}
	return std::nullopt;
}

Entry WithValues(Entry target, const std::map<std::string, std::string>& values,
                 const IsoDate& today) {
	for (const auto& field : Settables()) {
		auto given = values.find(field.name);
		if (given != values.end())
			target = *field.set(target, given->second, today);
	}
	return target;
}

int RunStep(const Command& self, const args::Parsed& line, CliContext& ctx) {
	auto by = CountGiven(line, 1);
	if (not by.has_value())
		return UsageError(ctx, self, by.error());
	int delta = self.name == "+" ? by.value() : -by.value();
	IsoDate today = ctx.Today();
	size_t titles = 0;
	std::vector<Entry> at_total;
	int code = ChangeTitles(self, line, ctx, [&](const Entry& e) {
		++titles;
		if (delta > 0 and AtTotal(e))
			at_total.push_back(e);
		return StepProgress(e, delta, today);
	});
	if (line.Has("q") or at_total.size() != titles)
		return code;
	for (const auto& e : at_total)
		ctx.err << std::format("{} is at its total, {} {}. If there are more: catalist set TITLE "
		                       "total=N\n",
		                       DisplayName(e), e.total_units, UnitName(e.type, e.total_units));
	return code;
}

int RunSet(const Command& self, const args::Parsed& line, CliContext& ctx) {
	auto first_field = std::ranges::find_if(line.operands, IsAssignment);
	auto request = ReadRefsIn(self, line, ctx, {line.operands.begin(), first_field});
	if (request.stop)
		return *request.stop;
	if (first_field == line.operands.end())
		return UsageError(ctx, self, "nothing to change: give a FIELD=VALUE such as score=8");
	IsoDate today = ctx.Today();
	auto values = ReadAssignments({first_field, line.operands.end()}, today);
	if (not values.has_value())
		return UsageError(ctx, self, values.error());
	if (not Load(ctx))
		return 1;
	auto found = Resolve(ctx, request.refs, request.how);
	if (not found)
		return 1;
	auto current = TakeForWrite(ctx, *found);
	if (not current)
		return 1;
	for (const auto& each : *current) {
		if (auto reason = RefusedValues(each, values.value(), today))
			return Fail(ctx, *reason);
	}
	return ApplyTo(ctx, line, *current,
	               [&](const Entry& e) { return WithValues(e, values.value(), today); });
}

// The status each of the status commands sets.
const std::map<std::string, WatchStatus> kStatusCommands = {
    {"done", WatchStatus::kCompleted},  {"hold", WatchStatus::kOnHold},
    {"drop", WatchStatus::kDropped},    {"plan", WatchStatus::kPlanToWatch},
    {"resume", WatchStatus::kWatching}, {"rewatch", WatchStatus::kRewatching}};

int RunStatus(const Command& self, const args::Parsed& line, CliContext& ctx) {
	WatchStatus status = kStatusCommands.at(self.name);
	std::optional<int> score;
	if (line.Has("score")) {
		score = args::ParseCount(line.Get("score"));
		if (not score or *score > 10)
			return UsageError(ctx, self,
			                  std::format("--score '{}' is not 0 to 10", line.Get("score")));
	}
	IsoDate today = ctx.Today();
	return ChangeTitles(
	    self, line, ctx,
	    [&](const Entry& e) {
		    Entry next = SetStatus(e, status, today);
		    return score ? SetScore(next, *score, today) : next;
	    },
	    [&](const Entry& e) { return StatusRefusal(e, status); });
}

// Asks before titles are removed. False, with the reason on standard error,
// when the answer is not yes or when there is nobody to ask.
bool RemovalConfirmed(CliContext& ctx, const std::vector<Entry>& gone, bool ask) {
	for (size_t i = 0; gone.size() > 1 and i < gone.size(); ++i)
		ctx.err << "  " << refs::Of(gone[i]) << "  " << DisplayName(gone[i]) << "\n";
	std::string what = gone.size() == 1 ? std::format("{} ({})", DisplayName(gone[0]), refs::Of(gone[0]))
	                                    : std::format("these {} titles", gone.size());
	if (not ask) {
		Fail(ctx, std::format("nobody to ask here: pass -f to remove {}", what));
		return false;
	}
	ctx.err << std::format("Remove {} from your lists? [y/N]: ", what) << std::flush;
	std::string answer;
	std::getline(ctx.in, answer);
	answer = utf8::Lowercase(answer);
	if (answer == "y" or answer == "yes")
		return true;
	ctx.err << "Cancelled.\n";
	return false;
}

int RunRemove(const Command& self, const args::Parsed& line, CliContext& ctx) {
	auto request = ReadRefs(self, line, ctx);
	if (request.stop)
		return *request.stop;
	if (not Load(ctx))
		return 1;
	auto found = Resolve(ctx, request.refs, request.how);
	if (not found)
		return 1;
	if (not line.Has("f") and not RemovalConfirmed(ctx, *found, request.how.ask))
		return 1;
	auto current = TakeForWrite(ctx, *found);
	if (not current)
		return 1;
	std::vector<journal::Change> changes;
	for (const auto& each : *current) {
		ctx.db.RemoveEntry(each.type, each.id);
		changes.push_back({each.type, each.id, each, std::nullopt});
	}
	return SaveAndReport(ctx, line, changes);
}

const std::vector<WatchStatus> kInProgress = {WatchStatus::kWatching, WatchStatus::kRewatching};

// done, hold, drop, plan and resume differ in their name and their words.
Command StatusCommand(const std::string& name, const std::string& summary,
                      const std::string& details) {
	std::vector<args::Option> options = {kQuiet};
	std::vector<WatchStatus> pick_from = kInProgress;
	if (name == "resume")
		pick_from = {WatchStatus::kOnHold, WatchStatus::kDropped};
	else if (name == "rewatch")
		pick_from = {WatchStatus::kCompleted};
	else if (name == "plan")
		pick_from = {};
	if (name == "done")
		options.insert(options.begin(), {0, "score", "N", "give it a score as well, 0 to 10"});
	return {.name = name,
	        .summary = summary,
	        .details = details + "\n" + kRefsHelp + "\n" + kChangeFormat,
	        .options = WithRefOptions(options),
	        .operands = pick_from.empty() ? "REF..." : "[REF...]",
	        .pick_from = pick_from,
	        .run = RunStatus};
}

Command StepCommand(const std::string& name, const std::string& alias, const std::string& summary,
                    const std::string& details) {
	return {.name = name,
	        .aliases = {alias},
	        .summary = summary,
	        .details = details + "\n" + kRefsHelp + "\n" + kChangeFormat,
	        .options = WithRefOptions({{'n', "", "N", "count N instead of one"}, kQuiet}),
	        .operands = "[REF...]",
	        .pick_from = kInProgress,
	        .run = RunStep};
}

}  // namespace

std::vector<Command> TrackingCommands() {
	return {
	    StepCommand("+", "inc", "Count one more episode or chapter",
	                "The first one starts a title, and the last one completes it.\n"),
	    StepCommand("-", "dec", "Count one less episode or chapter",
	                "Going back below the total reopens a completed title.\n"),
	    {.name = "set",
	     .summary = "Set fields of a title",
	     .details =
	         std::string(
	             "The fields: title, status, score (0 clears it), progress, total,\n"
	             "volumes, rewatched, start and finish (dates such as 2026-10-20, or\n"
	             "today; an empty value, as in finish=, clears one). Reaching the total completes a\n"
	             "title, as + does. All values are checked before anything changes.\n\n"
	             "The REF is a ref such as a:12, or part of a title.\n\n") +
	         kChangeFormat,
	     .options = WithRefOptions({kQuiet}),
	     .operands = "REF FIELD=VALUE...",
	     .run = RunSet},
	    StatusCommand("done", "Mark titles completed",
	                  "Sets the finish date, and the progress to the total when it is known.\n"),
	    StatusCommand("hold", "Put titles on hold", ""),
	    StatusCommand("drop", "Mark titles dropped", ""),
	    StatusCommand("plan", "Mark titles planned", ""),
	    StatusCommand("resume", "Take titles up again",
	                  "Sets the status to watching (reading, for a manga). A completed title\n"
	                  "is refused: use rewatch.\n"),
	    StatusCommand("rewatch", "Watch or read titles again",
	                  "Sets the status to rewatching (rereading, for a manga). A completed title\n"
	                  "starts again from the first episode or chapter and keeps the dates of\n"
	                  "the first time; reaching the total completes it again, one more to\n"
	                  "its rewatch count. Holding or dropping it returns it to completed. A title\n"
	                  "not completed yet is refused: use resume.\n"),
	    {.name = "rm",
	     .summary = "Remove titles",
	     .details =
	         std::string("At a terminal rm asks first. Where nobody can be asked, in a script,\n"
	                     "it removes nothing without -f. 'undo' brings a removed title back\n"
	                     "under its ref.\n\n") +
	         kRefsHelp + "\n" + kChangeFormat,
	     .options = WithRefOptions({{'f', "", "", "remove without asking"}, kQuiet}),
	     .operands = "REF...",
	     .run = RunRemove},
	};
}

}  // namespace catalist::cli
