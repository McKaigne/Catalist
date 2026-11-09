#include "catalist/cli/commands/commands.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "catalist/cli/args.hpp"
#include "catalist/cli/output.hpp"
#include "catalist/storage/malParser.hpp"

namespace catalist::cli {

int runImport(const commandArgs& args, commandContext& ctx) {
  database& db = ctx.db;
  iFileSystem& fileSystem = ctx.fileSystem;
  if (args.empty()) {
    emitError("usage", "usage: catalist import [--clean] <file.xml ...>",
              false);
    return 2;
  }

  bool clean = false;
  std::vector<std::string> files;

  for (const auto& a : args) {
    if (a == "--clean" || a == "--wipe")
      clean = true;
    else
      files.push_back(a);
  }

  if (files.empty()) {
    emitError("usage", "no XML files specified for import.", false);
    return 2;
  }

  size_t totalImported = 0;
  for (const auto& f : files) {
    size_t count = malParser::importXml(fileSystem, f, db, clean);
    std::cout << "Successfully ingested " << count << " records from: " << f
              << "\n";
    totalImported += count;
  }

  std::cout << "Import complete. " << totalImported
            << " total records loaded.\n";
  return 0;
}

}  // namespace catalist::cli
