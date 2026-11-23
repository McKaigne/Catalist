#ifndef CATALIST_CLI_ARGS_HPP_
#define CATALIST_CLI_ARGS_HPP_

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "core/result.hpp"

// The command-line syntax, after the POSIX Utility Syntax Guidelines: options
// are one letter (`-a`, grouped as `-am`, with a value as `-n 3` or `-n3`) or
// a long name (`--all`, `--sort=title`, `--sort title`). `--` ends them. A
// lone `-` is an operand. Pure text in, options and operands out.
namespace catalist::args {

// An option a command takes. `letter` is 0 or `name` is empty when it has
// only the other form; `value` names what follows it (empty for a flag).
struct Option {
	char letter = 0;
	std::string name;
	std::string value;
	std::string help;
};

struct Parsed {
	// By the option's long name, or its letter when it has none. A flag's
	// value is empty.
	std::map<std::string, std::string> options;
	std::vector<std::string> operands;

	[[nodiscard]] bool Has(const std::string& name) const { return options.count(name) > 0; }
	// The option's value, empty when it was not given.
	[[nodiscard]] std::string Get(const std::string& name) const;
};

// Fails, saying which argument is wrong, on an option not in `options`, a
// missing value, or a value given to a flag.
enum class Placement { kBeforeOperands, kAnywhere };

[[nodiscard]] Result<Parsed> Parse(const std::vector<std::string>& arguments,
                                   const std::vector<Option>& options,
                                   Placement placement = Placement::kBeforeOperands);

// A count: digits only, and few enough of them to fit an int.
[[nodiscard]] std::optional<int> ParseCount(const std::string& text);

}  // namespace catalist::args

#endif  // CATALIST_CLI_ARGS_HPP_
