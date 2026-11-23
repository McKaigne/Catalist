#ifndef CATALIST_CLI_CLI_HPP_
#define CATALIST_CLI_CLI_HPP_

#include <ctime>
#include <functional>
#include <istream>
#include <string>
#include <vector>

#include "cli/args.hpp"
#include "core/date.hpp"
#include "storage/catalogue.hpp"
#include "web/http.hpp"

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
	// How AniList is reached.
	web::Fetcher net;
	// Someone is at a terminal: add and rm may ask a question.
	bool interactive = false;
	// Write colours (a terminal, and NO_COLOR unset).
	bool color = false;
	// The terminal's width, for tables; 0 when output goes to a pipe or a
	// file, which gets tab-separated lines instead.
	int columns = 0;
	// The command line as it was typed; RunCli sets it. It names the undo
	// step of what the command changes.
	std::string typed{};

	[[nodiscard]] IsoDate Today() const { return IsoDate::FromLocalTm(clock()); }
	// When a change is made, for the undo history.
	[[nodiscard]] std::string Stamp() const { return StampOf(clock()); }
};

// One command of the command line. The table of them (see Commands) is what
// the parser and the help read.
struct Command {
	std::string name{};
	std::vector<std::string> aliases{};
	std::string summary{};
	std::string details{};
	std::vector<args::Option> options{};
	// The operands as the usage line shows them: "REF...".
	std::string operands{};
	int (*run)(const Command& self, const args::Parsed& line, CliContext& ctx) = nullptr;
};

[[nodiscard]] const std::vector<Command>& Commands();
// "ls [-amqr] [-s STATUS] [--sort=KEY] [QUERY]": the command with its
// options and operands, as the help shows it.
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
