#ifndef CATALIST_CLI_COMMANDS_COMMANDS_HPP_
#define CATALIST_CLI_COMMANDS_COMMANDS_HPP_

#include <string>
#include <vector>

#include "catalist/cli/commandContext.hpp"

// One handler per CLI command; each lives in cli/commands/<name>.cpp.
namespace catalist::cli {

using commandArgs = std::vector<std::string>;

int runLs(const commandArgs& args, commandContext& ctx);
int runCount(const commandArgs& args, commandContext& ctx);
int runShow(const commandArgs& args, commandContext& ctx);
int runAdd(const commandArgs& args, commandContext& ctx);
int runSet(const commandArgs& args, commandContext& ctx);
int runRm(const commandArgs& args, commandContext& ctx);
int runRandom(const commandArgs& args, commandContext& ctx);
int runStats(const commandArgs& args, commandContext& ctx);
int runExport(const commandArgs& args, commandContext& ctx);
int runImport(const commandArgs& args, commandContext& ctx);
int runSearch(const commandArgs& args, commandContext& ctx);
int runConfig(const commandArgs& args, commandContext& ctx);
int runCompletion(const commandArgs& args, commandContext& ctx);
int runLogin(const commandArgs& args, commandContext& ctx);
int runLogout(const commandArgs& args, commandContext& ctx);
int runSync(const commandArgs& args, commandContext& ctx);
// `import mal|anilist ...`: pulls a list without sending anything back.
int importFromAccount(const commandArgs& args, commandContext& ctx);

}  // namespace catalist::cli

#endif  // CATALIST_CLI_COMMANDS_COMMANDS_HPP_
