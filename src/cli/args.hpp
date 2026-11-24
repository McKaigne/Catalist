#ifndef CATALIST_CLI_ARGS_HPP_
#define CATALIST_CLI_ARGS_HPP_

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "core/result.hpp"

namespace catalist::args {

struct Option {
	char letter = 0;
	std::string name;
	std::string value;
	std::string help;
};

struct Parsed {
	std::map<std::string, std::string> options;
	std::vector<std::string> operands;

	[[nodiscard]] bool Has(const std::string& name) const { return options.count(name) > 0; }
	[[nodiscard]] std::string Get(const std::string& name) const;
};

enum class Placement { kBeforeOperands, kAnywhere };

[[nodiscard]] Result<Parsed> Parse(const std::vector<std::string>& arguments,
                                   const std::vector<Option>& options,
                                   Placement placement = Placement::kBeforeOperands);

[[nodiscard]] std::optional<int> ParseCount(const std::string& text);

}  // namespace catalist::args

#endif  // CATALIST_CLI_ARGS_HPP_
