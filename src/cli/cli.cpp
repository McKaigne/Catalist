#include "cli/cli.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <ostream>

#include "cli/common.hpp"
#include "core/version.hpp"

namespace catalist {

namespace {

using cli::didYouMean;
using cli::usageError;

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
	// The helps start in one column, two past the longest label.
	size_t width = 18;
	for (const auto& each : options)
		width = std::max(width, optionLabel(each).size() + 2);
	for (const auto& each : options)
		ctx.out << fmt::format("  {:<{}}{}\n", optionLabel(each), width, each.help);
}

void printHelp(cliContext& ctx) {
	const std::string& program = ctx.program;
	ctx.out << fmt::format(
	    "Catalist {} - track anime and manga from the terminal.\n\n"
	    "Usage: {} [OPTION...] COMMAND [ARG...]\n\nCommands:\n",
	    kVersionString, program);
	for (const auto& each : commands())
		if (not each.hidden)
			ctx.out << fmt::format("  {:<12}{}\n", each.name, each.summary);
	ctx.out << "\nOptions, before the command:\n";
	for (const auto& each : globalOptions())
		ctx.out << fmt::format("  {:<18}{}\n", optionLabel(each), each.help);
	ctx.out << fmt::format(
	    "\nName a title by its ref, the first column of '{0} ls' (a:12 is an anime,\n"
	    "m:3 a manga), or by part of its title:\n"
	    "  {0} show a:12\n"
	    "  {0} + frieren\n"
	    "  {0} set a:12 score=9\n"
	    "  {0} undo\n"
	    "\nOptions come before the titles. Run '{0} help COMMAND' for the options of\n"
	    "a command. '{0}' alone lists what you are watching or reading, new\n"
	    "episodes and chapters first. Your lists are kept in\n{1}.\n",
	    program, ctx.db.getDataDir().string());
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
	return usageError(ctx, self,
	                  fmt::format("no command '{}'{}", name, didYouMean(name, commandNames())));
}

// A command line wrong before its command: no command, or one not known.
int commandError(cliContext& ctx, const std::string& what) {
	ctx.err << fmt::format(
	    "error: {}\nUsage: {} [OPTION...] COMMAND [ARG...]\nRun '{} help' to see the commands.\n",
	    what, ctx.program, ctx.program);
	return 2;
}

// The program alone at a terminal: what is being watched or read, the
// titles with new episodes or chapters first.
int runDefault(cliContext& ctx) {
	const command* list = findCommand("ls");
	auto line = args::parse({"-s", "watching", "--sort=new"}, list->options);
	return list->run(*list, line.value(), ctx);
}

// What the options before the command ask for: colours, and tables into a
// pipe. False on a WHEN that is not one.
bool applyGlobals(cliContext& ctx, const args::parsed& line) {
	std::string when = line.has("color") ? line.get("color") : "auto";
	if (when == "always")
		ctx.color = true;
	else if (when == "never")
		ctx.color = false;
	else if (when != "auto")
		return false;
	// A table into a pipe is as wide as the shell says, and has no colours
	// unless they are asked for.
	if (line.has("table") and ctx.columns == 0)
		ctx.columns = ctx.tableWidth;
	if (line.has("offline"))
		ctx.offline = true;
	return true;
}

int unknownCommand(cliContext& ctx, const std::string& first) {
	return commandError(
	    ctx, fmt::format("unknown command '{}'{}", first, didYouMean(first, commandNames())));
}

}  // namespace

// The commands in the order the help lists them, each group from its file
// (see cli/common.hpp), then help.
const std::vector<command>& commands() {
	static const std::vector<command> all = [] {
		std::vector<command> table;
		for (const auto& group :
		     {cli::listingCommands(), cli::trackingCommands(), cli::onlineCommands(),
		      cli::accountCommands(), cli::syncCommands(), cli::transferCommands(),
		      cli::historyCommands(), cli::setupCommands()})
			table.insert(table.end(), group.begin(), group.end());
		table.push_back({"help",
		                 {},
		                 "Show help for a command",
		                 "",
		                 {},
		                 "[COMMAND]",
		                 runHelp,
		                 operandKind::kCommands});
		return table;
	}();
	return all;
}

const std::vector<args::option>& globalOptions() {
	static const std::vector<args::option> all = {
	    {0,
	     "color",
	     "WHEN",
	     "colours: auto (at a terminal), always or never",
	     {"auto", "always", "never"}},
	    {'t', "table", "", "draw tables into a pipe as well ($COLUMNS wide)"},
	    {0, "offline", "", "keep what the sites are to be told until 'sync'"},
	    {'V', "version", "", "print the version"},
	    {'h', "help", "", "show the commands"}};
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

int runCli(const std::vector<std::string>& args, cliContext& ctx) {
	ctx.typed.clear();
	for (const auto& word : args)
		ctx.typed += (ctx.typed.empty() ? "" : " ") + word;
	// -v is what many programs print their version with.
	std::vector<args::option> globals = globalOptions();
	globals.push_back({'v', "", "", ""});
	auto program = args::parse(args, globals);
	if (not program.ok())
		return commandError(ctx, program.errorMessage());
	const args::parsed& before = program.value();
	if (not applyGlobals(ctx, before))
		return commandError(
		    ctx, fmt::format("--color '{}' is not auto, always or never", before.get("color")));
	if (before.has("version") or before.has("v")) {
		ctx.out << "Catalist v" << kVersionString << "\n";
		return 0;
	}
	if (before.has("help")) {
		printHelp(ctx);
		return 0;
	}
	if (before.operands.empty())
		return ctx.columns > 0 ? runDefault(ctx) : commandError(ctx, "no command given");

	const std::string& first = before.operands[0];
	const command* cmd = findCommand(first);
	if (not cmd)
		return unknownCommand(ctx, first);
	std::vector<args::option> options = cmd->options;
	options.push_back(helpOption());
	auto line = args::parse({before.operands.begin() + 1, before.operands.end()}, options);
	// The scripts' own commands say nothing, whatever they are given.
	if (not line.ok())
		return cmd->hidden ? 0 : usageError(ctx, *cmd, line.errorMessage());
	if (line.value().has("help")) {
		printCommandHelp(ctx, *cmd);
		return 0;
	}
	return cmd->run(*cmd, line.value(), ctx);
}

}  // namespace catalist
