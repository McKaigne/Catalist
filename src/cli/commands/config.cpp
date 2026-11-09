#include "catalist/cli/commands/commands.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "catalist/cli/args.hpp"
#include "catalist/cli/output.hpp"

namespace catalist::cli {

int runConfig(const commandArgs& args, commandContext& ctx) {
  appConfig& config = ctx.config;
  iFileSystem& fileSystem = ctx.fileSystem;
  const auto& configPath = ctx.configPath;

  if (args.empty() || args[0] == "list") {
    std::cout << "mouse = " << (config.mouse ? "true" : "false") << "\n"
              << "theme = \"" << config.theme << "\"\n"
              << "default_media = \"" << config.defaultMedia << "\"\n"
              << "default_sort = \"" << config.defaultSort << "\"\n"
              << "reverse_sort = " << (config.reverseSort ? "true" : "false")
              << "\n";
    return 0;
  }

  std::string action = args[0];

  if (action == "get") {
    if (args.size() < 2) {
      emitError("usage", "usage: catalist config get <key>", false);
      return 2;
    }
    std::string key = args[1];
    if (key == "mouse")
      std::cout << (config.mouse ? "true" : "false") << "\n";
    else if (key == "theme")
      std::cout << config.theme << "\n";
    else if (key == "default_media" || key == "defaultMedia")
      std::cout << config.defaultMedia << "\n";
    else if (key == "default_sort" || key == "defaultSort")
      std::cout << config.defaultSort << "\n";
    else if (key == "reverse_sort" || key == "reverseSort")
      std::cout << (config.reverseSort ? "true" : "false") << "\n";
    else {
      emitError("config", "unknown config key '" + key + "'.", false);
      return 1;
    }
    return 0;
  }

  if (action == "set") {
    if (args.size() < 3) {
      emitError("usage", "usage: catalist config set <key> <value>", false);
      return 2;
    }
    std::string key = args[1];
    std::string val = args[2];

    if (key == "mouse")
      config.mouse = (val == "true" || val == "1");
    else if (key == "theme")
      config.theme = val;
    else if (key == "default_media" || key == "defaultMedia")
      config.defaultMedia = val;
    else if (key == "default_sort" || key == "defaultSort")
      config.defaultSort = val;
    else if (key == "reverse_sort" || key == "reverseSort")
      config.reverseSort = (val == "true" || val == "1");
    else {
      emitError("config", "unknown config key '" + key + "'.", false);
      return 1;
    }

    config.save(fileSystem, configPath);
    std::cout << "Set " << key << " = " << val << "\n";
    return 0;
  }

  emitError("usage",
            "unknown config action '" + action + "'. Expected: list, get, set.",
            false);
  return 2;
}

}  // namespace catalist::cli
