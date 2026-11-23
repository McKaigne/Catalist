#include "cli/cli.hpp"

#include <algorithm>
#include <format>

#include "cli/common.hpp"
#include "core/version.hpp"

namespace catalist {

namespace {

using cli::UsageError;

const Command* FindCommand(const std::string& name) {
	for (const auto& each : Commands()) {
		if (each.name == name or
		    std::find(each.aliases.begin(), each.aliases.end(), name) != each.aliases.end())
			return &each;
	}
	return nullptr;
}

// "-s STATUS", "    --sort=KEY", "-h, --help".
std::string OptionLabel(const args::Option& shown) {
	std::string label = shown.letter ? std::format("-{}", shown.letter) : "  ";
	if (not shown.name.empty())
		label += std::format("{} --{}", shown.letter ? "," : " ", shown.name);
	if (not shown.value.empty())
		label += std::format("{}{}", shown.name.empty() ? " " : "=", shown.value);
	return label;
}

void PrintCommandHelp(CliContext& ctx, const Command& cmd) {
	ctx.out << std::format("{}.\n\nUsage: catalist {}\n", cmd.summary, UsageLine(cmd));
	if (not cmd.aliases.empty()) {
		std::string also;
		for (const auto& alias : cmd.aliases)
			also += std::format("{}catalist {}", also.empty() ? "" : ", ", alias);
		ctx.out << std::format("Also:  {}\n", also);
	}
	if (not cmd.details.empty())
		ctx.out << "\n" << cmd.details;
	ctx.out << "\nOptions:\n";
	std::vector<args::Option> options = cmd.options;
	options.push_back(HelpOption());
	// The helps start in one column, two past the longest label.
	size_t width = 18;
	for (const auto& each : options)
		width = std::max(width, OptionLabel(each).size() + 2);
	for (const auto& each : options)
		ctx.out << std::format("  {:<{}}{}\n", OptionLabel(each), width, each.help);
}

void PrintHelp(CliContext& ctx) {
	ctx.out << std::format(
	    "Catalist {} - track anime and manga from the terminal.\n\n"
	    "Usage: catalist [OPTION...] COMMAND [ARG...]\n\nCommands:\n",
	    kVersionString);
	for (const auto& each : Commands())
		ctx.out << std::format("  {:<12}{}\n", each.name, each.summary);
	ctx.out << "\nOptions, before the command:\n";
	for (const auto& each : GlobalOptions())
		ctx.out << std::format("  {:<18}{}\n", OptionLabel(each), each.help);
	ctx.out << std::format(
	    "\nName a title by its ref, the first column of 'catalist ls' (a:12 is an anime,\n"
	    "m:3 a manga), or by part of its title:\n"
	    "  catalist show a:12\n"
	    "  catalist + frieren\n"
	    "  catalist set a:12 score=9\n"
	    "  catalist undo\n"
	    "\nOptions come before the titles. Run 'catalist help COMMAND' for the options of\n"
	    "a command. 'catalist' alone lists what you are watching or reading, the\n"
	    "latest changed first. Your lists are kept in\n{}.\n",
	    ctx.db.data_dir().string());
}

int RunHelp(const Command& self, const args::Parsed& line, CliContext& ctx) {
	if (line.operands.empty()) {
		PrintHelp(ctx);
		return 0;
	}
	if (line.operands.size() > 1)
		return UsageError(ctx, self, "name one command");
	const std::string& name = line.operands[0];
	if (const Command* asked = FindCommand(name)) {
		PrintCommandHelp(ctx, *asked);
		return 0;
	}
	return UsageError(ctx, self, std::format("no command '{}'", name));
}

// A command line wrong before its command: no command, or one not known.
int CommandError(CliContext& ctx, const std::string& what) {
	ctx.err << std::format(
	    "error: {}\nUsage: catalist [OPTION...] COMMAND [ARG...]\nRun 'catalist help' to see the "
	    "commands.\n",
	    what);
	return 2;
}

// The program alone at a terminal: what is being watched or read, the
// title changed last first.
int RunDefault(CliContext& ctx) {
	return cli::PrintList(ctx, {.statuses = {WatchStatus::kWatching, WatchStatus::kRewatching},
	                            .key = SortKey::kUpdated});
}

}  // namespace

// The commands in the order the help lists them, each group from its file
// (see cli/common.hpp), then help.
const std::vector<Command>& Commands() {
	static const std::vector<Command> kAll = [] {
		std::vector<Command> table;
		for (const auto& group :
		     {cli::ListingCommands(), cli::TrackingCommands(), cli::OnlineCommands(),
		      cli::TransferCommands(), cli::HistoryCommands()})
			table.insert(table.end(), group.begin(), group.end());
		table.push_back({.name = "help",
		                 .summary = "Show help for a command",
		                 .operands = "[COMMAND]",
		                 .run = RunHelp});
		return table;
	}();
	return kAll;
}

const std::vector<args::Option>& GlobalOptions() {
	static const std::vector<args::Option> kAll = {{'V', "version", "", "print the version"},
	                                               {'h', "help", "", "show the commands"}};
	return kAll;
}

const args::Option& HelpOption() {
	static const args::Option kHelp{'h', "help", "", "show this help"};
	return kHelp;
}

std::string UsageLine(const Command& shown) {
	std::string flags;
	std::string valued;
	for (const auto& each : shown.options) {
		if (each.value.empty() and each.letter)
			flags += each.letter;
		else if (each.value.empty())
			valued += std::format(" [--{}]", each.name);
		else if (each.letter)
			valued += std::format(" [-{} {}]", each.letter, each.value);
		else
			valued += std::format(" [--{}={}]", each.name, each.value);
	}
	return std::format("{}{}{}{}", shown.name, flags.empty() ? "" : std::format(" [-{}]", flags),
	                   valued, shown.operands.empty() ? "" : std::format(" {}", shown.operands));
}

int RunCli(const std::vector<std::string>& args, CliContext& ctx) {
	ctx.typed.clear();
	for (const auto& word : args)
		ctx.typed += (ctx.typed.empty() ? "" : " ") + word;
	// -v is what many programs print their version with.
	std::vector<args::Option> globals = GlobalOptions();
	globals.push_back({'v', "", "", ""});
	auto program = args::Parse(args, globals);
	if (not program.has_value())
		return CommandError(ctx, program.error());
	const args::Parsed& before = program.value();
	if (before.Has("version") or before.Has("v")) {
		ctx.out << "Catalist v" << kVersionString << "\n";
		return 0;
	}
	if (before.Has("help")) {
		PrintHelp(ctx);
		return 0;
	}
	if (before.operands.empty())
		return ctx.columns > 0 ? RunDefault(ctx) : CommandError(ctx, "no command given");

	const std::string& first = before.operands[0];
	const Command* cmd = FindCommand(first);
	if (not cmd)
		return CommandError(ctx, std::format("unknown command '{}'", first));
	std::vector<args::Option> options = cmd->options;
	options.push_back(HelpOption());
	auto line = args::Parse({before.operands.begin() + 1, before.operands.end()}, options,
	                        args::Placement::kAnywhere);
	if (not line.has_value())
		return UsageError(ctx, *cmd, line.error());
	if (line.value().Has("help")) {
		PrintCommandHelp(ctx, *cmd);
		return 0;
	}
	return cmd->run(*cmd, line.value(), ctx);
}

}  // namespace catalist
