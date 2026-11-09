#include "catalist/cli/output.hpp"

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>

#include <unistd.h>

#include "catalist/core/jsonLite.hpp"

namespace catalist {

namespace ansi {
const std::string kReset = "\033[0m";
const std::string kBold = "\033[1m";
const std::string kDim = "\033[2m";
const std::string kMauve = "\033[38;2;203;166;247m";
const std::string kLavender = "\033[38;2;180;190;254m";
const std::string kPeach = "\033[38;2;250;179;135m";
const std::string kYellow = "\033[38;2;249;226;175m";
const std::string kGreen = "\033[38;2;166;227;161m";
const std::string kBlue = "\033[38;2;137;180;250m";
const std::string kRed = "\033[38;2;243;139;168m";
const std::string kSubtext = "\033[38;2;166;173;200m";
}  // namespace ansi

}  // namespace catalist

namespace catalist::cli {

using jsonLite::escapeJsonString;

namespace {

std::string describeCandidates(const std::vector<entry>& candidates) {
  std::string listing;
  for (const auto& c : candidates) {
    listing += "  - [" + std::to_string(c.id) + "] " + c.title + " (" +
               (c.type == mediaType::kAnime ? "anime" : "manga") + ")\n";
  }
  return listing;
}

}  // namespace

bool shouldColor(bool interactive) {
  return interactive && std::getenv("NO_COLOR") == nullptr;
}

using jsonLite::escapeJsonString;

void emitError(const std::string& code, const std::string& message,
                      bool jsonMode) {
  if (jsonMode) {
    std::cerr << "{\"error\":{\"code\":\"" << escapeJsonString(code)
              << "\",\"message\":\"" << escapeJsonString(message) << "\"}}\n";
  } else {
    if (shouldColor(isatty(STDERR_FILENO))) {
      std::cerr << ansi::kRed << ansi::kBold << "error: " << ansi::kReset
                << message << "\n";
    } else {
      std::cerr << "error: " << message << "\n";
    }
  }
}

void reportResolveError(const resolveError& failure, bool jsonMode,
                               bool listCandidates) {
  if (failure.kind == resolveFailure::kNotFound) {
    emitError("notFound", "entry not found matching '" + failure.query + "'.",
              jsonMode);
    return;
  }
  std::string message =
      "ambiguous query '" + failure.query + "'. Matches multiple entries";
  if (listCandidates)
    message += ":\n" + describeCandidates(failure.candidates);
  else
    message += ".";
  emitError("ambiguous", message, jsonMode);
}

std::string formatJsonEntry(const entry& e) {
  std::ostringstream ss;
  ss << "{"
     << "\"id\":" << e.id << ","
     << "\"malId\":" << e.malId << ","
     << "\"anilistId\":" << e.anilistId << ","
     << "\"title\":\"" << escapeJsonString(e.title) << "\","
     << "\"type\":\"" << (e.type == mediaType::kAnime ? "anime" : "manga")
     << "\","
     << "\"status\":\"" << statusToString(e.status, e.type) << "\","
     << "\"rating\":" << e.score << ","
     << "\"progress\":" << e.progress << ","
     << "\"totalUnits\":" << e.totalUnits << ","
     << "\"volumes\":" << e.volumes << ","
     << "\"totalVolumes\":" << e.totalVolumes << ","
     << "\"rewatched\":" << e.rewatched << ","
     << "\"startDate\":\"" << escapeJsonString(formatDate(e.startDate)) << "\","
     << "\"finishDate\":\"" << escapeJsonString(formatDate(e.finishDate)) << "\","
     << "\"notes\":\"" << escapeJsonString(e.notes) << "\","
     << "\"updatedAt\":\"" << escapeJsonString(formatDate(e.updatedAt)) << "\""
     << "}";
  return ss.str();
}

std::string formatJsonEntries(
    const std::vector<entry>& entries) {
  std::ostringstream ss;
  ss << "[\n";
  for (size_t i = 0; i < entries.size(); ++i) {
    ss << "  " << formatJsonEntry(entries[i]);
    if (i + 1 < entries.size())
      ss << ",";
    ss << "\n";
  }
  ss << "]\n";
  return ss.str();
}

void printUsage() {
  std::cout
      << "Usage: catalist [command] [options]\n\n"
      << "Query Commands:\n"
      << "  ls                   List watchlist entries (table, plain, CSV, or JSON)\n"
      << "  show <id|title|->    Show detailed information for an entry\n"
      << "  count                Count matching entries\n"
      << "  random               Pick a random entry matching filters\n"
      << "  search <query>       Search MyAnimeList online (yay-style interactive add)\n"
      << "  stats                Display watch and read telemetry\n\n"
      << "Mutation Commands:\n"
      << "  add <title> [flags]  Add a new anime or manga entry\n"
      << "  set <id|title|->     Update fields on existing records\n"
      << "  rm <id|title|->      Remove entries (-y to skip confirmation)\n\n"
      << "Management & Setup:\n"
      << "  export <json|csv>    Dump entire database to stdout\n"
      << "  import <file.xml>    Ingest MyAnimeList XML export\n"
      << "  config [list|get|set] Inspect or change preferences (settings.json)\n"
      << "  completion <shell>   Generate shell completion script (bash, zsh, fish)\n"
      << "  --tab <id>           Open the TUI on a tab (anime, manga, stats,\n"
      << "                       settings)\n"
      << "  --version, -v        Display version info\n"
      << "  --help, -h           Show this message\n\n"
      << "Listing & Filtering Flags:\n"
      << "  -t, --type <anime|manga>  Filter media type\n"
      << "  -s, --status <status>     Filter: watching, reading, completed, on-hold, dropped, planned\n"
      << "  -r, --rating <spec>       Filter rating: 9, '>=9', '<=7', '6..8', 'unrated'\n"
      << "  --unrated                 Filter entries where rating == 0\n"
      << "  -y, --year <YYYY>         Filter by start year\n"
      << "  -q, --search <query>      Filter by title substring match\n"
      << "  --sort <field>            Sort: date, title, status, rating, progress\n"
      << "  -R, --reverse             Invert sort direction\n"
      << "  -l, --limit <N>           Limit output to first N records\n\n"
      << "Progress & Stepping Flags (set):\n"
      << "  -p, --progress <val>      Set absolute, or step: add [N], a [N], sub [N], s [N], +N, -N\n\n"
      << "Formatting Flags:\n"
      << "  --plain                   Print bare titles, one per line\n"
      << "  --ids                     Print bare IDs, one per line\n"
      << "  --csv                     Output RFC 4180 CSV\n"
      << "  --json                    Output structured JSON\n";
}

void printSingleDetail(const entry& e, bool jsonMode, bool color) {
  if (jsonMode) {
    std::cout << formatJsonEntry(e) << "\n";
    return;
  }

  auto tag = [&](const std::string& name) {
    if (color)
      std::cout << ansi::kBold << ansi::kSubtext << std::left << std::setw(14)
                << name << ansi::kReset;
    else
      std::cout << std::left << std::setw(14) << name;
  };

  tag("Title:");
  std::cout << (color ? ansi::kBold : "") << e.title
            << (color ? ansi::kReset : "") << "\n";
  tag("Type:");
  std::cout << (e.type == mediaType::kAnime ? "Anime" : "Manga") << "\n";
  tag("Status:");
  std::cout << statusToString(e.status, e.type) << "\n";
  tag("Rating:");
  std::cout << (e.score > 0 ? (std::to_string(e.score) + "/10") : "Unrated")
            << "\n";
  tag("Progress:");
  std::cout << e.progress << " / "
            << (e.totalUnits > 0 ? std::to_string(e.totalUnits) : "?")
            << " units\n";
  if (e.type == mediaType::kManga && (e.volumes > 0 || e.totalVolumes > 0)) {
    tag("Volumes:");
    std::cout << e.volumes << " / "
              << (e.totalVolumes > 0 ? std::to_string(e.totalVolumes) : "?")
              << "\n";
  }
  tag("Dates:");
  std::cout << (e.startDate ? e.startDate->toString() : "-") << " -> "
            << (e.finishDate ? e.finishDate->toString() : "-") << "\n";
  tag("MAL ID:");
  std::cout << (e.malId > 0 ? std::to_string(e.malId) : "Local") << "\n";
  tag("Local ID:");
  std::cout << e.id << "\n";
  tag("Notes:");
  std::cout << (e.notes.empty() ? "-" : e.notes) << "\n";
}

}  // namespace catalist::cli
