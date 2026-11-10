#include "catalist/cli/dispatcher.hpp"

#include <iostream>
#include <random>

#include "catalist/cli/commands/commands.hpp"
#include "catalist/cli/output.hpp"
#include "catalist/core/version.hpp"

namespace catalist::cli {

size_t pickRandomIndex(size_t count) {
  std::random_device seed;
  std::mt19937 generator(seed());
  std::uniform_int_distribution<size_t> distribution(0, count - 1);
  return distribution(generator);
}

const std::vector<commandEntry>& commandTable() {
  static const std::vector<commandEntry> table = {
      {"ls", runLs},         {"count", runCount},
      {"show", runShow},     {"add", runAdd},
      {"set", runSet},       {"rm", runRm},
      {"random", runRandom}, {"stats", runStats},
      {"export", runExport}, {"import", runImport},
      {"search", runSearch}, {"find", runSearch},
      {"config", runConfig}, {"completion", runCompletion},
      {"login", runLogin},   {"logout", runLogout},
      {"sync", runSync},
  };
  return table;
}

int dispatch(int argc, char* argv[], commandContext& ctx) {
  if (argc <= 1)
    return 0;

  std::string command = argv[1];
  if (command == "--version" || command == "-v") {
    std::cout << "Catalist v" << kVersionString << " (C++17 / FTXUI)\n";
    return 0;
  }
  if (command == "--help" || command == "-h") {
    printUsage();
    return 0;
  }

  commandArgs args(argv + 2, argv + argc);
  for (const auto& known : commandTable()) {
    if (command == known.name)
      return known.handler(args, ctx);
  }
  emitError(
      "usage",
      "unknown command '" + command + "'. Run 'catalist --help' for usage.",
      false);
  return 2;
}

}  // namespace catalist::cli
