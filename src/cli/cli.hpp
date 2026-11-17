#ifndef CATALIST_CLI_CLI_HPP_
#define CATALIST_CLI_CLI_HPP_

#include <ctime>
#include <filesystem>
#include <functional>
#include <istream>
#include <optional>
#include <string>
#include <vector>

#include "cli/args.hpp"
#include "core/date.hpp"
#include "storage/catalogue.hpp"
#include "web/tracker.hpp"

namespace catalist {

// What a command-line run works with. main builds it from the real
// terminal; the tests build it from string streams and a fake web.
struct CliContext {
	Catalogue& db;
	std::istream& in;
	std::ostream& out;
	std::ostream& err;
	// The local date and time: main passes the computer's clock, the tests
	// a fixed one.
	std::function<std::tm()> clock;
	// The sites, and how they are reached.
	web::Link net;
	// Where the logins are kept (see storage/accounts.hpp).
	std::filesystem::path accounts_file{"accounts"};
	// Reads a password: a line of `in`, not shown at a terminal.
	std::function<std::optional<std::string>(std::istream&)> read_secret =
	    [](std::istream& in) -> std::optional<std::string> {
		std::string line;
		return std::getline(in, line) ? std::optional<std::string>(line) : std::nullopt;
	};
	// Someone is at a terminal: add and rm may ask a question.
	bool interactive = false;
	// Write colours (a terminal, and NO_COLOR unset).
	bool color = false;
	// The terminal's width, for tables; 0 when output goes to a pipe or a
	// file, which gets tab-separated lines instead.
	int columns = 0;
	// --offline or CATALIST_OFFLINE: what the sites are to be told waits.
	bool offline = false;
	// How wide a table drawn into a pipe (--table) is: $COLUMNS, else 80.
	int table_width = 80;
	// The command line as it was typed; RunCli sets it. It names the undo
	// step of what the command changes.
	std::string typed{};

	[[nodiscard]] IsoDate Today() const { return IsoDate::FromLocalTm(clock()); }
	// When a change is made, for the undo history.
	[[nodiscard]] std::string Stamp() const { return StampOf(clock()); }
};

// What a command's operands are, so the shell can complete them: nothing,
// titles of the lists by ref or by name, one such title and then FIELD=VALUE
// words, a part of a title, text nothing is known about, files, one of the
// command's `words`, or a command.
enum class OperandKind { kNone, kRefs, kRefThenFields, kTitle, kText, kFiles, kWords, kCommands };

// One command of the command line. The table of them (see commands) is what
// the parser, the help, the manual page and the completion scripts all read.
struct Command {
	std::string name{};
	std::vector<std::string> aliases{};
	std::string summary{};
	std::string details{};
	std::vector<args::Option> options{};
	// The operands as the usage line shows them: "REF...".
	std::string operands{};
	int (*run)(const Command& self, const args::Parsed& line, CliContext& ctx) = nullptr;
	OperandKind completes = OperandKind::kNone;
	// The words to choose from, for kWords.
	std::vector<std::string> words{};
	// Left out of the help: the completion scripts' own commands.
	bool hidden = false;
};

[[nodiscard]] const std::vector<Command>& Commands();
// "ls [-amqr] [-s STATUS] [--sort=KEY] [QUERY]": the command with its
// options and operands, as the help and the manual page show it.
[[nodiscard]] std::string UsageLine(const Command& shown);
// The options of the program itself, which come before the command.
[[nodiscard]] const std::vector<args::Option>& GlobalOptions();
// The option every command takes besides its own.
[[nodiscard]] const args::Option& HelpOption();

// Runs `catalist <args>` and returns the exit code: 0 on success, 1 when
// the work failed, 2 when the command line is wrong. Data goes to `out`;
// questions, notes and errors go to `err`. See `catalist help`.
int RunCli(const std::vector<std::string>& args, CliContext& ctx);

}  // namespace catalist

#endif  // CATALIST_CLI_CLI_HPP_
