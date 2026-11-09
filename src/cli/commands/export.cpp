#include "catalist/cli/commands/commands.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "catalist/cli/args.hpp"
#include "catalist/cli/output.hpp"
#include "catalist/storage/csvCodec.hpp"

namespace catalist::cli {

int runExport(const commandArgs& args, commandContext& ctx) {
  database& db = ctx.db;
  if (args.empty()) {
    emitError("usage",
              "usage: catalist export <json|csv> [--type <anime|manga>]",
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
                << formatJsonEntries(db.getEntries(mediaType::kAnime))
                << ",\n  \"manga\": "
                << formatJsonEntries(db.getEntries(mediaType::kManga))
                << "\n}\n";
    }
    return 0;
  }

  if (formatStr == "csv") {
    std::cout
        << "#id,mal_id,anilist_id,type,title,status,score,progress,total_units,volumes,total_volumes,rewatched,start_date,finish_date,notes,updated_at\n";
    auto dumpList = [](const std::vector<entry>& list) {
      for (const auto& e : list) {
        std::cout << e.id << "," << e.malId << "," << e.anilistId << ","
                  << (e.type == mediaType::kAnime ? "anime" : "manga") << ","
                  << csvCodec::escapeField(e.title) << ","
                  << csvCodec::escapeField(statusToString(e.status, e.type)) << ","
                  << e.score << "," << e.progress << "," << e.totalUnits << ","
                  << e.volumes << "," << e.totalVolumes << "," << e.rewatched
                  << "," << csvCodec::escapeField(formatDate(e.startDate)) << ","
                  << csvCodec::escapeField(formatDate(e.finishDate)) << "," << csvCodec::escapeField(e.notes) << ","
                  << csvCodec::escapeField(formatDate(e.updatedAt)) << "\n";
      }
    };

    if (!filterType.has_value() || filterType.value() == mediaType::kAnime) {
      dumpList(db.getEntries(mediaType::kAnime));
    }
    if (!filterType.has_value() || filterType.value() == mediaType::kManga) {
      dumpList(db.getEntries(mediaType::kManga));
    }
    return 0;
  }

  emitError("usage",
            "unsupported export format '" + formatStr +
                "'. Expected 'json' or 'csv'.",
            false);
  return 2;
}

}  // namespace catalist::cli
