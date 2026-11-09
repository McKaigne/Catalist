#ifndef CATALIST_CLI_DISPATCHER_HPP_
#define CATALIST_CLI_DISPATCHER_HPP_

#include <string>
#include <vector>

#include "catalist/cli/commandContext.hpp"
#include "catalist/cli/commands/commands.hpp"

namespace catalist::cli {

using commandHandler = int (*)(const commandArgs& args, commandContext& ctx);

struct commandEntry {
  std::string name;
  commandHandler handler;
};

[[nodiscard]] const std::vector<commandEntry>& commandTable();

// Handles --version and --help, then runs the matching command. Returns the
// process exit code: 0 success, 1 runtime failure, 2 usage error.
int dispatch(int argc, char* argv[], commandContext& ctx);

}  // namespace catalist::cli

#endif  // CATALIST_CLI_DISPATCHER_HPP_
