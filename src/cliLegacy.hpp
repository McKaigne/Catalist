#ifndef CATALIST_CLI_LEGACY_HPP_
#define CATALIST_CLI_LEGACY_HPP_

#include <iosfwd>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "cli.hpp"

// The commands not yet moved to the new command line (see cli.hpp): they
// keep their old syntax until they are. This file goes when the last one
// has moved.
namespace catalist {

// Each command's name and one-line summary.
[[nodiscard]] std::vector<std::pair<std::string, std::string>> legacySummaries();
// Prints the help of one of them; false when it is not one.
bool printLegacyHelp(const std::string& name, std::ostream& out);
// Runs `args` when it starts with one of them; nothing when it does not.
[[nodiscard]] std::optional<int> runLegacyCli(const std::vector<std::string>& args,
                                              cliContext& ctx);

}  // namespace catalist

#endif  // CATALIST_CLI_LEGACY_HPP_
