#ifndef CATALIST_CLI_ARGS_HPP_
#define CATALIST_CLI_ARGS_HPP_

#include <iosfwd>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "core/result.hpp"

// The command-line syntax, after the POSIX Utility Syntax Guidelines: options
// are one letter (`-a`, grouped as `-am`, with a value as `-n 3` or `-n3`) or
// a long name (`--json`, `--sort=title`, `--sort title`); they come before
// the operands; `--` ends them, and so does the first operand. A lone `-` is
// an operand. Pure text in, options and operands out.
namespace catalist::args {

// An option a command takes. `letter` is 0 or `name` is empty when it has
// only the other form; `value` names what follows it (empty for a flag).
struct option {
	char letter = 0;
	std::string name;
	std::string value;
	std::string help;
	// The values there are to choose from, when they are only a few: the
	// shell completes them. The command checks what it is given.
	std::vector<std::string> choices{};
};

// How the option is written: "--sort", or "-n" when it has no long name.
[[nodiscard]] std::string spelled(const option& taken);

struct parsed {
	// By the option's long name, or its letter when it has none. A flag's
	// value is empty.
	std::map<std::string, std::string> options;
	std::vector<std::string> operands;

	[[nodiscard]] bool has(const std::string& name) const { return options.count(name) > 0; }
	// The option's value, empty when it was not given.
	[[nodiscard]] std::string get(const std::string& name) const;
};

// Fails, saying which argument is wrong, on an option not in `options`, a
// missing value, or a value given to a flag.
[[nodiscard]] result<parsed> parse(const std::vector<std::string>& arguments,
                                   const std::vector<option>& options);

// The operands with each `-` replaced by the lines of `in` (blank lines are
// skipped), for commands that take their operands from a pipe.
[[nodiscard]] std::vector<std::string> withLinesFor(const std::vector<std::string>& operands,
                                                    std::istream& in);

// How many single-character edits turn one word into the other.
[[nodiscard]] size_t editDistance(const std::string& from, const std::string& to);

// The known word a typo was probably meant to be: at most two edits away.
[[nodiscard]] std::optional<std::string> closestTo(const std::string& typed,
                                                   const std::vector<std::string>& known);

// A count: digits only, and few enough of them to fit an int.
[[nodiscard]] std::optional<int> parseCount(const std::string& text);

}  // namespace catalist::args

#endif  // CATALIST_CLI_ARGS_HPP_
