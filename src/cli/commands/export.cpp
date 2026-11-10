#include "catalist/cli/commands/commands.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "catalist/cli/args.hpp"
#include "catalist/cli/output.hpp"
#include "catalist/storage/csvCodec.hpp"
#include "catalist/storage/malParser.hpp"

namespace catalist::cli {

namespace {

std::string withoutTrailingNewline(std::string text) {
  if (!text.empty() && text.back() == '\n')
    text.pop_back();
  return text;
}

}  // namespace

int runExport(const commandArgs& args, commandContext& ctx) {
  database& db = ctx.db;
  if (args.empty()) {
    emitError("usage",
              "usage: catalist export <json|csv|xml> [--type <anime|manga>]",
              false);
    return 2;
  }

  std::string formatStr = args[0];
  std::transform(formatStr.begin(), formatStr.end(), formatStr.begin(),
                 ::tolower);

  std::optional<mediaType> filterType;
  for (size_t i = 1; i < args.size(); ++i) {
    if (args[i] == "--type" || args[i] == "-t") {
      if (++i < args.size() && parseMediaTypeArgument(args[i]))
        filterType = parseMediaTypeArgument(args[i]);
    }
  }

  if (formatStr == "json") {
    if (filterType.has_value()) {
      std::cout << formatJsonEntries(db.getEntries(filterType.value()));
    } else {
      std::cout << "{\n  \"anime\": "
                << withoutTrailingNewline(
                       formatJsonEntries(db.getEntries(mediaType::kAnime)))
                << ",\n  \"manga\": "
                << withoutTrailingNewline(
                       formatJsonEntries(db.getEntries(mediaType::kManga)))
                << "\n}\n";
    }
    return 0;
  }

  if (formatStr == "csv") {
    std::vector<entry> exported;
    for (auto type : {mediaType::kAnime, mediaType::kManga}) {
      if (filterType.value_or(type) != type)
        continue;
      const auto& list = db.getEntries(type);
      exported.insert(exported.end(), list.begin(), list.end());
    }
    std::cout << csvCodec::encodeEntries(exported);
    return 0;
  }

  if (formatStr == "xml") {
    mediaType type = filterType.value_or(mediaType::kAnime);
    auto exported = malParser::exportXml(db.getEntries(type), type);
    std::cout << exported.xml;
    if (exported.skipped > 0)
      std::cerr << "note: " << exported.skipped
                << " entries without a MyAnimeList id were left out.\n";
    return 0;
  }

  emitError("usage",
            "unsupported export format '" + formatStr +
                "'. Expected 'json', 'csv' or 'xml'.",
            false);
  return 2;
}

}  // namespace catalist::cli
