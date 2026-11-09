#include "catalist/cli/commands/commands.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "catalist/cli/args.hpp"
#include "catalist/cli/output.hpp"

namespace catalist::cli {

int runCount(const commandArgs& args, commandContext& ctx) {
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
  if (jsonMode) {
    std::cout << "{\"count\":" << entries.size() << "}\n";
  } else {
    std::cout << entries.size() << "\n";
  }
  return 0;
}

}  // namespace catalist::cli
