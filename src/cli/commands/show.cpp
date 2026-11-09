#include "catalist/cli/commands/commands.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "catalist/cli/args.hpp"
#include "catalist/cli/output.hpp"

namespace catalist::cli {

int runShow(const commandArgs& args, commandContext& ctx) {
  database& db = ctx.db;
  if (args.empty()) {
    emitError("usage",
              "missing entry ID or title for 'catalist show <id|title|->'.",
              false);
    return 2;
  }

  bool jsonMode = false;
  std::string target;

  for (const auto& a : args) {
    if (a == "--json")
      jsonMode = true;
    else if (target.empty())
      target = a;
  }

  if (target.empty()) {
    emitError("usage", "missing target identifier or title.", jsonMode);
    return 2;
  }

  std::vector<std::string> queries = collectQueries(target);

  if (queries.empty()) {
    emitError("input", "no queries received from stdin.", jsonMode);
    return 1;
  }

  int exitCode = 0;
  for (size_t qIdx = 0; qIdx < queries.size(); ++qIdx) {
    const auto& q = queries[qIdx];
    auto resolved = resolveEntry(db, q);
    if (!resolved.ok()) {
      reportResolveError(resolved.errorValue(), jsonMode, true);
      exitCode = 1;
      continue;
    }
    const entry& match = resolved.value();

    printSingleDetail(match, jsonMode);
    if (qIdx + 1 < queries.size())
      std::cout << "\n";
  }

  return exitCode;
}

}  // namespace catalist::cli
