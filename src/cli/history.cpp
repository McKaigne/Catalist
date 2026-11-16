#include <fmt/format.h>

#include <ostream>

#include "cli/common.hpp"

namespace catalist::cli {

namespace {

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

}  // namespace

std::vector<command> historyCommands() {
	return {
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
	};
}

}  // namespace catalist::cli
