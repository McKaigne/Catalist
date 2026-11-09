#include "catalist/cli/commands/commands.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "catalist/cli/args.hpp"
#include "catalist/cli/output.hpp"

namespace catalist::cli {

int runRandom(const commandArgs& args, commandContext& ctx) {
  database& db = ctx.db;
  cliFilterOptions defaults;
  auto parsed = parseCommonFilters(args, defaults);
  if (!parsed.ok()) {
    emitError("usage", parsed.errorValue().message,
              parsed.errorValue().jsonErrors);
    return 2;
  }
  const cliFilterOptions& opts = parsed.value();
  bool jsonMode = opts.jsonErrors;

  auto entries = collectAndFilterEntries(db, opts);
  if (entries.empty()) {
    emitError("notFound", "no entries match the specified filters.", jsonMode);
    return 1;
  }

  const auto& picked = entries[ctx.randomIndex(entries.size())];

  if (opts.format == outputFormat::kPlain) {
    std::cout << picked.title << "\n";
    return 0;
  }

  if (opts.format == outputFormat::kIds) {
    std::cout << picked.id << "\n";
    return 0;
  }

  if (opts.format == outputFormat::kJson) {
    std::cout << formatJsonEntry(picked) << "\n";
    return 0;
  }

  printSingleDetail(picked, false);
  return 0;
}

}  // namespace catalist::cli
