#include "catalist/cli/commands/commands.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "catalist/cli/args.hpp"
#include "catalist/cli/output.hpp"
#include "catalist/net/remoteSearch.hpp"
#include "catalist/core/jsonLite.hpp"

namespace catalist::cli {

namespace {

// Remote APIs cap a page at 25 results; out-of-range values are clamped and
// non-numbers ignored.
std::optional<size_t> parseResultLimit(const std::string& raw) {
  try {
    return static_cast<size_t>(std::clamp(std::stoi(raw), 1, 25));
  } catch (...) {
    return std::nullopt;
  }
}

}  // namespace

using jsonLite::escapeJsonString;

int runSearch(const commandArgs& args, commandContext& ctx) {
  database& db = ctx.db;
  iHttpClient& http = ctx.http;
  if (args.empty()) {
    emitError(
        "usage",
        "usage: catalist search <query> [--type <anime|manga>] [--limit <N>]",
        false);
    return 2;
  }

  std::string queryStr;
  mediaType type = mediaType::kAnime;
  size_t limit = 10;
  outputFormat format = outputFormat::kTable;
  bool jsonMode = false;

  for (size_t i = 0; i < args.size(); ++i) {
    if (args[i] == "--json") {
      format = outputFormat::kJson;
      jsonMode = true;
    } else if (args[i] == "--plain") {
      format = outputFormat::kPlain;
    } else if (args[i] == "--type" || args[i] == "-t") {
      if (++i < args.size())
        type = parseMediaTypeArgument(args[i]).value_or(type);
    } else if (args[i] == "--limit" || args[i] == "-l") {
      if (++i < args.size())
        limit = parseResultLimit(args[i]).value_or(limit);
    } else if (queryStr.empty() && args[i].rfind("-", 0) != 0) {
      queryStr = args[i];
    }
  }

  if (queryStr.empty()) {
    emitError("usage", "missing search query for 'catalist search <query>'.",
              jsonMode);
    return 2;
  }

  std::string err;
  auto results = remoteSearchEngine(http).query(type, queryStr, err, limit);

  if (results.empty()) {
    emitError("notFound",
              err.empty() ? ("no results found for '" + queryStr + "'.") : err,
              jsonMode);
    return 1;
  }

  if (format == outputFormat::kPlain) {
    for (const auto& r : results)
      std::cout << r.title << "\n";
    return 0;
  }

  if (format == outputFormat::kJson) {
    std::cout << "[\n";
    for (size_t i = 0; i < results.size(); ++i) {
      const auto& r = results[i];
      std::cout << "  {"
                << "\"malId\":" << r.malId << ","
                << "\"title\":\"" << escapeJsonString(r.title) << "\","
                << "\"type\":\""
                << (r.type == mediaType::kAnime ? "anime" : "manga") << "\","
                << "\"episodes\":" << r.totalUnits << ","
                << "\"year\":\"" << r.year << "\","
                << "\"score\":" << r.score << "}"
                << (i + 1 < results.size() ? "," : "") << "\n";
    }
    std::cout << "]\n";
    return 0;
  }

  bool color = shouldColor();
  std::cout << "\n";
  if (color)
    std::cout << ansi::kBold << ansi::kMauve;
  std::cout << "  #  " << std::left << std::setw(38) << "TITLE" << "  "
            << std::setw(7) << "TYPE" << "  " << std::setw(9) << "EPISODES"
            << "  " << std::setw(6) << "YEAR" << "  " << std::setw(6) << "SCORE"
            << "  "
            << "MAL ID\n";
  if (color)
    std::cout << ansi::kReset << ansi::kDim << ansi::kSubtext;
  std::cout << " --  " << std::string(38, '-') << "  " << std::string(7, '-')
            << "  " << std::string(9, '-') << "  " << std::string(6, '-')
            << "  " << std::string(6, '-') << "  "
            << "------\n";
  if (color)
    std::cout << ansi::kReset;

  for (size_t i = 0; i < results.size(); ++i) {
    const auto& r = results[i];
    std::string tDisplay = r.title;
    if (tDisplay.size() > 36)
      tDisplay = tDisplay.substr(0, 33) + "...";
    std::string epStr =
        (r.totalUnits > 0 ? (std::to_string(r.totalUnits) + " eps")
                          : "ongoing");

    std::cout << " " << std::right << std::setw(2) << (i + 1) << "  "
              << std::left << std::setw(38) << tDisplay << "  " << std::setw(7)
              << (r.type == mediaType::kAnime ? "anime" : "manga") << "  "
              << std::setw(9) << epStr << "  " << std::setw(6)
              << (r.year.empty() ? "-" : r.year) << "  " << std::setw(6)
              << std::fixed << std::setprecision(2) << r.score << "  "
              << r.malId << "\n";
  }

  if (!IS_TTY())
    return 0;

  std::cout << "\nSelect number to add [1-" << results.size()
            << ", or 'q' to quit]: " << std::flush;
  std::string choice;
  if (!std::getline(std::cin, choice) || choice == "q" || choice == "Q" ||
      choice.empty()) {
    std::cout << "Canceled.\n";
    return 0;
  }

  int pick = 0;
  try {
    pick = std::stoi(choice);
  } catch (...) {
    return 0;
  }
  if (pick < 1 || pick > static_cast<int>(results.size())) {
    std::cout << "Invalid selection.\n";
    return 1;
  }

  const auto& selected = results[pick - 1];

  std::cout
      << "Select status [1: Watching, 2: Completed, 3: Plan to Watch] (default: 1): "
      << std::flush;
  std::string stChoice;
  watchStatus st = watchStatus::kWatching;
  if (std::getline(std::cin, stChoice) && !stChoice.empty()) {
    if (stChoice == "2")
      st = watchStatus::kCompleted;
    else if (stChoice == "3")
      st = watchStatus::kPlanToWatch;
  }

  entry e;
  e.id = selected.malId;
  e.malId = selected.malId;
  e.type = selected.type;
  e.title = selected.title;
  e.status = st;
  e.score = 0;
  e.progress = (st == watchStatus::kCompleted) ? selected.totalUnits : 0;
  e.totalUnits = selected.totalUnits;
  e.startDate = isoDate::parse(selected.year + "-01-01");

  db.addEntry(e);
  db.saveAll();

  if (color)
    std::cout << ansi::kGreen << ansi::kBold;
  std::cout << "Added " << (e.type == mediaType::kAnime ? "anime" : "manga")
            << " '" << e.title << "' [MAL ID " << e.malId
            << "] to watchlist.\n";
  if (color)
    std::cout << ansi::kReset;

  return 0;
}

}  // namespace catalist::cli
