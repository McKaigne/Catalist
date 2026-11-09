#include "catalist/cli/commands/commands.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "catalist/cli/args.hpp"
#include "catalist/cli/output.hpp"

namespace catalist::cli {

int runStats(const commandArgs& args, commandContext& ctx) {
  database& db = ctx.db;
  bool jsonMode = false;
  std::optional<mediaType> filterType;

  for (size_t i = 0; i < args.size(); ++i) {
    if (args[i] == "--json")
      jsonMode = true;
    else if (args[i] == "--type" || args[i] == "-t") {
      if (++i < args.size() && parseMediaTypeArgument(args[i]))
        filterType = parseMediaTypeArgument(args[i]);
    }
  }

  auto getStatsObj = [&](mediaType type) {
    std::ostringstream ss;
    ss << "{"
       << "\"total\":" << db.getEntries(type).size() << ","
       << "\"daysSpent\":" << std::fixed << std::setprecision(1)
       << db.daysSpent(type) << ","
       << "\"meanScore\":" << db.averageScore(type) << ","
       << "\"completedPercent\":" << db.completedPercentage(type) << ","
       << "\"progressUnits\":" << db.totalProgress(type) << ","
       << "\"rewatched\":" << db.totalRewatched(type) << ","
       << "\"watching\":" << db.countByStatus(type, watchStatus::kWatching)
       << ","
       << "\"completed\":" << db.countByStatus(type, watchStatus::kCompleted)
       << ","
       << "\"onHold\":" << db.countByStatus(type, watchStatus::kOnHold) << ","
       << "\"dropped\":" << db.countByStatus(type, watchStatus::kDropped) << ","
       << "\"planned\":" << db.countByStatus(type, watchStatus::kPlanToWatch);
    if (type == mediaType::kManga) {
      ss << ",\"volumes\":" << db.totalVolumes(mediaType::kManga);
    }
    ss << "}";
    return ss.str();
  };

  if (jsonMode) {
    if (filterType.has_value()) {
      std::cout << getStatsObj(filterType.value()) << "\n";
    } else {
      std::cout << "{\"anime\":" << getStatsObj(mediaType::kAnime)
                << ",\"manga\":" << getStatsObj(mediaType::kManga) << "}\n";
    }
    return 0;
  }

  bool color = shouldColor(ctx.interactive);

  auto printMediaStats = [&](mediaType type) {
    std::string name = (type == mediaType::kAnime ? "Anime" : "Manga");
    size_t tot = db.getEntries(type).size();
    size_t cWatch = db.countByStatus(type, watchStatus::kWatching);
    size_t cComp = db.countByStatus(type, watchStatus::kCompleted);
    size_t cHold = db.countByStatus(type, watchStatus::kOnHold);
    size_t cDrop = db.countByStatus(type, watchStatus::kDropped);
    size_t cPlan = db.countByStatus(type, watchStatus::kPlanToWatch);

    if (color)
      std::cout << ansi::kBold
                << (type == mediaType::kAnime ? ansi::kLavender : ansi::kPeach)
                << "=== " << name << " Telemetry ===" << ansi::kReset << "\n";
    else
      std::cout << "=== " << name << " Telemetry ===\n";

    std::cout << "  Total Records:      " << tot << "\n";
    std::cout << "  Days Logged:        " << std::fixed << std::setprecision(1)
              << db.daysSpent(type) << " days\n";
    std::cout << "  Mean Rating:        " << std::fixed << std::setprecision(2)
              << db.averageScore(type) << " / 10\n";
    std::cout << "  Completed:          " << std::fixed << std::setprecision(1)
              << db.completedPercentage(type) << "%\n";
    std::cout << "  " << (type == mediaType::kAnime ? "Episodes" : "Chapters")
              << " Logged:   " << db.totalProgress(type) << "\n";
    if (type == mediaType::kManga) {
      std::cout << "  Volumes Logged:     " << db.totalVolumes(mediaType::kManga)
                << "\n";
    }
    std::cout << "  Status Breakdown:   " << (color ? ansi::kGreen : "")
              << cWatch << " "
              << (type == mediaType::kAnime ? "watching" : "reading") << ", "
              << (color ? ansi::kBlue : "") << cComp << " completed, "
              << (color ? ansi::kYellow : "") << cHold << " on-hold, "
              << (color ? ansi::kRed : "") << cDrop << " dropped, "
              << (color ? ansi::kSubtext : "") << cPlan << " planned"
              << (color ? ansi::kReset : "") << "\n\n";
  };

  if (!filterType.has_value() || filterType.value() == mediaType::kAnime) {
    printMediaStats(mediaType::kAnime);
  }
  if (!filterType.has_value() || filterType.value() == mediaType::kManga) {
    printMediaStats(mediaType::kManga);
  }

  return 0;
}

}  // namespace catalist::cli
