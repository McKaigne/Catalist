#ifndef CATALIST_CLI_HPP_
#define CATALIST_CLI_HPP_

#include <functional>
#include <iosfwd>
#include <string>
#include <vector>

#include "anilist.hpp"
#include "catalogue.hpp"

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
};

// Runs `catalist <args>` for a non-empty argument list and returns the exit
// code: 0 on success, 1 when the work failed, 2 when the command line is
// wrong. See `catalist help`.
int runCli(const std::vector<std::string>& args, cliContext& ctx);

}  // namespace catalist

#endif  // CATALIST_CLI_HPP_
