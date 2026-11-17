#include <format>
#include <ostream>

#include "cli/common.hpp"

namespace catalist::cli {

namespace {

// undo and redo.
int RunReplay(const Command& self, const args::Parsed& line, CliContext& ctx) {
	bool redoing = self.name == "redo";
	if (not line.operands.empty())
		return UsageError(ctx, self, std::format("{} takes no operands", self.name));
	auto count = CountGiven(line, 1);
	if (not count.has_value())
		return UsageError(ctx, self, count.error());
	if (not LoadForWrite(ctx))
		return 1;

	bool force = line.Has("f");
	auto done = redoing ? ctx.db.Redo(count.value(), force) : ctx.db.Undo(count.value(), force);
	if (not done.has_value()) {
		Fail(ctx, done.error());
		if (not force)
			ctx.err << "Nothing was changed. With -f the step is taken all the same, where "
			           "it still can be.\n";
		return 1;
	}
	if (done.value().steps.empty()) {
		ctx.err << std::format("Nothing to {}.\n", self.name);
		return 0;
	}
	TellSites(ctx, done.value().changes);
	if (line.Has("q"))
		return 0;
	for (const auto& step : done.value().steps)
		ctx.err << std::format("{} '{}' ({}).\n", redoing ? "Redid" : "Undid", step.label,
		                       step.stamp);
	PrintChanges(ctx, done.value().changes);
	return 0;
}

int RunLog(const Command& self, const args::Parsed& line, CliContext& ctx) {
	if (not line.operands.empty())
		return UsageError(ctx, self, "log takes no operands");
	auto count = CountGiven(line, 0);
	if (not count.has_value())
		return UsageError(ctx, self, count.error());
	if (not Load(ctx))
		return 1;
	auto steps = ctx.db.History(count.value());
	if (not steps.has_value())
		return Fail(ctx, steps.error());

	auto state_of = [](const TxRecord& step) { return step.undone ? "undone" : "done"; };
	if (not line.Has("json") and ctx.columns > 0) {
		std::vector<std::vector<output::Styled>> rows;
		for (const auto& step : steps.value())
			rows.push_back({{std::to_string(step.id), output::kCyan},
			                {step.stamp},
			                {state_of(step), step.undone ? output::kDim : nullptr},
			                {std::to_string(step.changes)},
			                {step.label}});
		if (rows.empty())
			ctx.err << "Nothing has been changed yet.\n";
		else
			ctx.out << output::Table(
			    {{"TX", true}, {"WHEN"}, {"STATE"}, {"CHANGES", true}, {"COMMAND", false, true}},
			    rows, ctx.columns, ctx.color);
		return 0;
	}
	std::vector<std::vector<output::Cell>> rows;
	for (const auto& step : steps.value())
		rows.push_back({output::Number(step.id), output::Text(step.stamp),
		                output::Text(state_of(step)), output::Number(step.changes),
		                output::Text(step.label)});
	WriteRows(ctx, {"tx", "stamp", "state", "changes", "label"}, rows, line.Has("json"),
	          line.Has("header"));
	return 0;
}

Command ReplayCommand(const std::string& name, const std::string& summary,
                      const std::string& details) {
	return {.name = name,
	        .summary = summary,
	        .details = details,
	        .options = {{'n', "", "N", "N steps instead of one"},
	                    {'f', "", "", "also when a title has changed since"},
	                    kQuiet},
	        .run = RunReplay};
}

}  // namespace

std::vector<Command> HistoryCommands() {
	return {
	    ReplayCommand("undo", "Take back what the last command changed",
	                  "Takes back the newest step of 'log' that is not undone: what one\n"
	                  "command changed. A removed title comes back under its old ref. When a\n"
	                  "title is no longer as the step left it, nothing is taken back and the\n"
	                  "title is named; -f takes the step back all the same.\n"),
	    ReplayCommand("redo", "Make again what undo took back",
	                  "Steps can be made again until a command changes something new.\n"),
	    {.name = "log",
	     .summary = "List what can be undone",
	     .details = "The last 100 steps, newest first. Into a pipe each is one line of fields\n"
	                "with a tab between them: tx, stamp, state (done or undone), changes (how\n"
	                "many titles) and label (the command).\n",
	     .options = {{'n', "", "N", "the newest N steps only"},
	                 {0, "json", "", "print one JSON object a line"},
	                 {0, "header", "", "name the columns on the first line of a pipe"}},
	     .run = RunLog},
	};
}

}  // namespace catalist::cli
