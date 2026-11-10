#include "catalist/cli/commands/commands.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "catalist/cli/args.hpp"
#include "catalist/cli/output.hpp"
#include "catalist/core/json.hpp"

namespace catalist::cli {


int runRm(const commandArgs& args, commandContext& ctx) {
  database& db = ctx.db;
  if (args.empty()) {
    emitError("usage", "usage: catalist rm <id|title|-> [--yes]", false);
    return 2;
  }

  std::string target;
  bool forceYes = false;
  bool jsonMode = false;

  for (const auto& a : args) {
    if (a == "--yes" || a == "-y")
      forceYes = true;
    else if (a == "--json")
      jsonMode = true;
    else if (target.empty())
      target = a;
  }

  if (target.empty()) {
    emitError("usage", "missing target identifier or title for 'catalist rm'.",
              jsonMode);
    return 2;
  }

  std::vector<std::string> queries = collectQueries(target);

  if (queries.empty()) {
    emitError("input", "no targets received from stdin.", jsonMode);
    return 1;
  }

  int deletedCount = 0;
  for (const auto& q : queries) {
    auto resolved = resolveEntry(db, q);
    if (!resolved.ok()) {
      reportResolveError(resolved.errorValue(), jsonMode, false);
      continue;
    }
    const entry& match = resolved.value();

    const auto& e = match;

    if (!forceYes) {
      if (!ctx.interactive) {
        emitError(
            "safety",
            "deletion of '" + e.title +
                "' requires '--yes' when running in non-interactive pipeline.",
            jsonMode);
        return 1;
      }
      std::cout << "Delete '" << e.title << "' [ID " << e.id
                << "]? [y/N]: " << std::flush;
      std::string resp;
      if (!std::getline(std::cin, resp) || (resp != "y" && resp != "Y")) {
        std::cout << "Canceled.\n";
        continue;
      }
    }

    db.removeEntry(e.type, e.id);
    deletedCount++;

    if (jsonMode) {
      std::cout << "{\"deleted\":true,\"id\":" << e.id << ",\"title\":\""
                << json::escape(e.title) << "\"}\n";
    } else {
      bool color = shouldColor(ctx.interactive);
      std::cout << (color ? ansi::kRed : "") << "Deleted '" << e.title
                << "' [ID " << e.id << "]" << (color ? ansi::kReset : "")
                << "\n";
    }
  }

  if (deletedCount > 0) {
    db.saveAll();
    return 0;
  }
  return 1;
}

}  // namespace catalist::cli
