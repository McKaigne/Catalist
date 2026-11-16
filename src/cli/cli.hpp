#ifndef CATALIST_CLI_CLI_HPP_
#define CATALIST_CLI_CLI_HPP_

#include <functional>
#include <iosfwd>
#include <string>
#include <vector>

#include "cli/args.hpp"
#include "storage/catalogue.hpp"
#include "web/anilist.hpp"

namespace catalist {

// What a command-line run works with. main builds it from the real
// terminal; the tests build it from string streams and a fake web.
struct cliContext {
	catalogue& db;
	std::istream& in;
	std::ostream& out;
	std::ostream& err;
	std::function<isoDate()> today;
	anilist::poster post;
	// Someone is at a terminal: add and rm may ask a question.
	bool interactive = false;
	// Write colours (a terminal, and NO_COLOR unset).
	bool color = false;
	// The terminal's width, for tables; 0 when output goes to a pipe or a
	// file, which gets tab-separated lines instead.
	int columns = 0;
	// How wide a table drawn into a pipe (--table) is: $COLUMNS, else 80.
	int tableWidth = 80;
	// The local date and time, stamped on what a command changes.
	std::function<std::string()> nowStamp = [] { return std::string(); };
	// CATALIST_TX: commands run with the same label share one undo step.
	std::string txLabel{};
	// The name the program was run by (`catalist` or `cl`), for its messages.
	std::string program = "catalist";
	// A label for CATALIST_TX that no other run of commands has.
	std::function<std::string()> newTxLabel = [] { return std::string(); };
	// The command line as it was typed; runCli sets it. It names the undo
	// step of what the command changes.
	std::string typed{};
};

// What a command's operands are, so the shell can complete them: nothing,
// titles of the lists by ref or by name, one such title and then FIELD=VALUE
// words, a part of a title, text nothing is known about, files, one of the
// command's `words`, or a command.
enum class operandKind { kNone, kRefs, kRefThenFields, kTitle, kText, kFiles, kWords, kCommands };

// One command of the command line. The table of them (see commands) is what
// the parser, the help, the manual page and the completion scripts all read.
struct command {
	std::string name;
	std::vector<std::string> aliases;
	std::string summary;
	std::string details;
	std::vector<args::option> options;
	// The operands as the usage line shows them: "REF...".
	std::string operands;
	int (*run)(const command& self, const args::parsed& line, cliContext& ctx) = nullptr;
	operandKind completes = operandKind::kNone;
	// The words to choose from, for kWords.
	std::vector<std::string> words{};
	// Left out of the help: the completion scripts' own commands.
	bool hidden = false;
};

[[nodiscard]] const std::vector<command>& commands();
// "ls [-amqr] [-s STATUS] [--sort=KEY] [QUERY]": the command with its
// options and operands, as the help and the manual page show it.
[[nodiscard]] std::string usageLine(const command& shown);
// The options of the program itself, which come before the command.
[[nodiscard]] const std::vector<args::option>& globalOptions();
// The option every command takes besides its own.
[[nodiscard]] const args::option& helpOption();

// Runs `catalist <args>` and returns the exit code: 0 on success, 1 when
// the work failed, 2 when the command line is wrong. Data goes to `out`;
// questions, notes and errors go to `err`. See `catalist help`.
int runCli(const std::vector<std::string>& args, cliContext& ctx);

}  // namespace catalist

#endif  // CATALIST_CLI_CLI_HPP_
