#include "catalist/cli.hpp"
#include "catalist/malParser.hpp"
#include "catalist/theme.hpp"
#include "catalist/remoteSearch.hpp"

#include <iostream>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <cstdlib>
#include <random>
#include <chrono>

#if defined(_WIN32)
#include <io.h>
#define IS_TTY() (_isatty(_fileno(stdout)))
#else
#include <unistd.h>
#define IS_TTY() (isatty(STDOUT_FILENO))
#endif

namespace catalist {

namespace ansi {
  const std::string Reset   = "\033[0m";
  const std::string Bold    = "\033[1m";
  const std::string Dim     = "\033[2m";
  const std::string Mauve   = "\033[38;2;203;166;247m";
  const std::string Lavender= "\033[38;2;180;190;254m";
  const std::string Peach   = "\033[38;2;250;179;135m";
  const std::string Yellow  = "\033[38;2;249;226;175m";
  const std::string Green   = "\033[38;2;166;227;161m";
  const std::string Blue    = "\033[38;2;137;180;250m";
  const std::string Red     = "\033[38;2;243;139;168m";
  const std::string Subtext = "\033[38;2;166;173;200m";
}

static bool shouldColor() {
  return IS_TTY() && (std::getenv("NO_COLOR") == nullptr);
}

static std::string escapeJsonString(const std::string& str) {
  std::ostringstream ss;
  for (char c : str) {
    switch (c) {
      case '"': ss << "\\\""; break;
      case '\\': ss << "\\\\"; break;
      case '\b': ss << "\\b"; break;
      case '\f': ss << "\\f"; break;
      case '\n': ss << "\\n"; break;
      case '\r': ss << "\\r"; break;
      case '\t': ss << "\\t"; break;
      default:
        if ('\x00' <= c && c <= '\x1f') {
          ss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
        } else {
          ss << c;
        }
    }
  }
  return ss.str();
}

static void emitError(const std::string& code, const std::string& message, bool jsonMode) {
  if (jsonMode) {
    std::cerr << "{\"error\":{\"code\":\"" << escapeJsonString(code)
              << "\",\"message\":\"" << escapeJsonString(message) << "\"}}\n";
  } else {
    if (shouldColor()) {
      std::cerr << ansi::Red << ansi::Bold << "error: " << ansi::Reset << message << "\n";
    } else {
      std::cerr << "error: " << message << "\n";
    }
  }
}

bool CliDispatcher::parseRatingSpec(const std::string& spec, int& minScore, int& maxScore, bool& unratedOnly) {
  minScore = -1;
  maxScore = -1;
  unratedOnly = false;

  std::string s = spec;
  s.erase(0, s.find_first_not_of(" \t\r\n\""));
  s.erase(s.find_last_not_of(" \t\r\n\"") + 1);

  if (s == "unrated" || s == "0") {
    unratedOnly = true;
    minScore = 0;
    maxScore = 0;
    return true;
  }

  if (s.rfind(">=", 0) == 0) {
    try {
      minScore = std::stoi(s.substr(2));
      maxScore = 10;
      return (minScore >= 0 && minScore <= 10);
    } catch (...) { return false; }
  }

  if (s.rfind(">", 0) == 0) {
    try {
      minScore = std::stoi(s.substr(1)) + 1;
      maxScore = 10;
      return (minScore >= 0 && minScore <= 10);
    } catch (...) { return false; }
  }

  if (s.rfind("<=", 0) == 0) {
    try {
      minScore = 0;
      maxScore = std::stoi(s.substr(2));
      return (maxScore >= 0 && maxScore <= 10);
    } catch (...) { return false; }
  }

  if (s.rfind("<", 0) == 0) {
    try {
      minScore = 0;
      maxScore = std::stoi(s.substr(1)) - 1;
      return (maxScore >= 0 && maxScore <= 10);
    } catch (...) { return false; }
  }

  size_t dots = s.find("..");
  if (dots != std::string::npos) {
    try {
      minScore = std::stoi(s.substr(0, dots));
      maxScore = std::stoi(s.substr(dots + 2));
      return (minScore >= 0 && maxScore <= 10 && minScore <= maxScore);
    } catch (...) { return false; }
  }

  try {
    int exact = std::stoi(s);
    if (exact >= 0 && exact <= 10) {
      minScore = exact;
      maxScore = exact;
      return true;
    }
  } catch (...) {}

  return false;
}

std::string CliDispatcher::formatJsonEntry(const Entry& e) {
  std::ostringstream ss;
  ss << "{"
     << "\"id\":" << e.id << ","
     << "\"malId\":" << e.malId << ","
     << "\"anilistId\":" << e.anilistId << ","
     << "\"title\":\"" << escapeJsonString(e.title) << "\","
     << "\"type\":\"" << (e.type == MediaType::Anime ? "anime" : "manga") << "\","
     << "\"status\":\"" << statusToString(e.status, e.type) << "\","
     << "\"rating\":" << e.score << ","
     << "\"progress\":" << e.progress << ","
     << "\"totalUnits\":" << e.totalUnits << ","
     << "\"volumes\":" << e.volumes << ","
     << "\"totalVolumes\":" << e.totalVolumes << ","
     << "\"rewatched\":" << e.rewatched << ","
     << "\"startDate\":\"" << escapeJsonString(e.startDate) << "\","
     << "\"finishDate\":\"" << escapeJsonString(e.finishDate) << "\","
     << "\"notes\":\"" << escapeJsonString(e.notes) << "\","
     << "\"updatedAt\":\"" << escapeJsonString(e.updatedAt) << "\""
     << "}";
  return ss.str();
}

std::string CliDispatcher::formatJsonEntries(const std::vector<Entry>& entries) {
  std::ostringstream ss;
  ss << "[\n";
  for (size_t i = 0; i < entries.size(); ++i) {
    ss << "  " << formatJsonEntry(entries[i]);
    if (i + 1 < entries.size()) ss << ",";
    ss << "\n";
  }
  ss << "]\n";
  return ss.str();
}

void CliDispatcher::printUsage() {
  std::cout << "Usage: catalist [command] [options]\n\n"
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
            << "  config [list|get|set] Inspect or modify ~/.config/catalist/config.toml\n"
            << "  completion <shell>   Generate shell completion script (bash, zsh, fish)\n"
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

int CliDispatcher::dispatch(int argc, char* argv[], Database& db, AppConfig& config) {
  if (argc <= 1) return 0;

  std::string command = argv[1];

  if (command == "--version" || command == "-v") {
    std::cout << "Catalist v0.2.0 (C++17 / FTXUI)\n";
    return 0;
  }
  if (command == "--help" || command == "-h") {
    printUsage();
    return 0;
  }

  std::vector<std::string> args;
  for (int i = 2; i < argc; ++i) {
    args.push_back(argv[i]);
  }

  if (command == "ls") return executeLs(args, db, config);
  if (command == "count") return executeCount(args, db);
  if (command == "show") return executeShow(args, db);
  if (command == "add") return executeAdd(args, db);
  if (command == "set") return executeSet(args, db);
  if (command == "rm") return executeRm(args, db);
  if (command == "random") return executeRandom(args, db);
  if (command == "stats") return executeStats(args, db);
  if (command == "export") return executeExport(args, db);
  if (command == "import") return executeImport(args, db);
  if (command == "search" || command == "find") return executeSearch(args, db);
  if (command == "config") return executeConfig(args, config);
  if (command == "completion") return executeCompletion(args);

  emitError("usage", "unknown command '" + command + "'. Run 'catalist --help' for usage.", false);
  return 2;
}

static bool parseCommonFilters(const std::vector<std::string>& args,
                               CliFilterOptions& opts,
                               bool& jsonMode,
                               std::string& err) {
  for (size_t i = 0; i < args.size(); ++i) {
    const std::string& arg = args[i];

    if (arg == "--json") {
      opts.format = OutputFormat::Json;
      jsonMode = true;
    } else if (arg == "--plain") {
      opts.format = OutputFormat::Plain;
    } else if (arg == "--ids") {
      opts.format = OutputFormat::Ids;
    } else if (arg == "--csv") {
      opts.format = OutputFormat::Csv;
    } else if (arg == "--reverse" || arg == "-R") {
      opts.reverseSort = true;
    } else if (arg == "--unrated") {
      opts.unratedOnly = true;
      opts.minScore = 0;
      opts.maxScore = 0;
    } else if (arg == "--type" || arg == "-t") {
      if (++i >= args.size()) { err = "missing argument for '--type'"; return false; }
      std::string t = args[i];
      std::transform(t.begin(), t.end(), t.begin(), ::tolower);
      if (t == "anime" || t == "a") opts.type = MediaType::Anime;
      else if (t == "manga" || t == "m") opts.type = MediaType::Manga;
      else { err = "invalid media type '" + args[i] + "'. Expected 'anime' or 'manga'."; return false; }
    } else if (arg == "--status" || arg == "-s") {
      if (++i >= args.size()) { err = "missing argument for '--status'"; return false; }
      opts.status = stringToStatus(args[i]);
    } else if (arg == "--rating" || arg == "-r" || arg == "--score") {
      if (++i >= args.size()) { err = "missing argument for '--rating'"; return false; }
      if (!CliDispatcher::parseRatingSpec(args[i], opts.minScore, opts.maxScore, opts.unratedOnly)) {
        err = "invalid rating specification '" + args[i] + "'. Expected e.g. 9, '>=9', '<=7', '6..8', or 'unrated'.";
        return false;
      }
    } else if (arg == "--year" || arg == "-y") {
      if (++i >= args.size()) { err = "missing argument for '--year'"; return false; }
      opts.year = args[i];
    } else if (arg == "--search" || arg == "-q") {
      if (++i >= args.size()) { err = "missing argument for '--search'"; return false; }
      opts.search = args[i];
    } else if (arg == "--limit" || arg == "-l") {
      if (++i >= args.size()) { err = "missing argument for '--limit'"; return false; }
      try {
        opts.limit = std::stoi(args[i]);
        if (opts.limit < 0) { err = "--limit must be non-negative"; return false; }
      } catch (...) { err = "invalid integer for '--limit': " + args[i]; return false; }
    } else if (arg == "--sort") {
      if (++i >= args.size()) { err = "missing argument for '--sort'"; return false; }
      std::string s = args[i];
      std::transform(s.begin(), s.end(), s.begin(), ::tolower);
      if (s == "score" || s == "rating") opts.sortKey = SortKey::Score;
      else if (s == "title") opts.sortKey = SortKey::Title;
      else if (s == "status") opts.sortKey = SortKey::Status;
      else if (s == "progress") opts.sortKey = SortKey::Progress;
      else if (s == "date" || s == "started" || s == "chrono") opts.sortKey = SortKey::Date;
      else if (s == "id") opts.sortKey = SortKey::Id;
      else { err = "unknown sort key '" + args[i] + "'. Expected: rating, title, status, progress, date, id."; return false; }
    } else {
      err = "unknown flag '" + arg + "'.";
      return false;
    }
  }
  return true;
}

static std::vector<Entry> collectAndFilterEntries(const Database& db, const CliFilterOptions& opts) {
  std::vector<Entry> pool;

  auto appendType = [&](MediaType t) {
    const auto& list = db.getEntries(t);
    for (const auto& e : list) {
      if (opts.status.has_value() && e.status != opts.status.value()) continue;
      if (opts.unratedOnly && e.score != 0) continue;
      if (opts.minScore >= 0 && opts.maxScore >= 0) {
        if (e.score < opts.minScore || e.score > opts.maxScore) continue;
      }
      if (!opts.year.empty()) {
        if (e.startDate.size() < 4 || e.startDate.substr(0, 4) != opts.year) continue;
      }
      if (!opts.search.empty()) {
        std::string titleLower = e.title;
        std::string qLower = opts.search;
        std::transform(titleLower.begin(), titleLower.end(), titleLower.begin(), ::tolower);
        std::transform(qLower.begin(), qLower.end(), qLower.begin(), ::tolower);
        if (titleLower.find(qLower) == std::string::npos) continue;
      }
      pool.push_back(e);
    }
  };

  if (!opts.type.has_value()) {
    appendType(MediaType::Anime);
    appendType(MediaType::Manga);
  } else {
    appendType(opts.type.value());
  }

  std::sort(pool.begin(), pool.end(), [opts](const Entry& a, const Entry& b) {
    bool res = false;
    switch (opts.sortKey) {
      case SortKey::Score:
        if (a.score != b.score) res = a.score > b.score;
        else res = a.title < b.title;
        break;
      case SortKey::Status:
        if (a.status != b.status) res = static_cast<int>(a.status) < static_cast<int>(b.status);
        else res = a.title < b.title;
        break;
      case SortKey::Title:
        res = a.title < b.title;
        break;
      case SortKey::Progress:
        if (a.progress != b.progress) res = a.progress > b.progress;
        else res = a.title < b.title;
        break;
      case SortKey::Date:
        if (a.startDate != b.startDate) res = a.startDate > b.startDate;
        else res = a.title < b.title;
        break;
      case SortKey::Id:
        res = a.id < b.id;
        break;
    }
    return opts.reverseSort ? !res : res;
  });

  if (opts.limit >= 0 && static_cast<size_t>(opts.limit) < pool.size()) {
    pool.resize(opts.limit);
  }

  return pool;
}

int CliDispatcher::executeLs(const std::vector<std::string>& args, Database& db, const AppConfig& config) {
  CliFilterOptions opts;
  opts.reverseSort = config.reverseSort;
  bool jsonMode = false;
  std::string err;

  if (!parseCommonFilters(args, opts, jsonMode, err)) {
    emitError("usage", err, jsonMode);
    return 2;
  }

  auto entries = collectAndFilterEntries(db, opts);

  if (opts.format == OutputFormat::Json) {
    std::cout << formatJsonEntries(entries);
    return 0;
  }

  if (opts.format == OutputFormat::Plain) {
    for (const auto& e : entries) {
      std::cout << e.title << "\n";
    }
    return 0;
  }

  if (opts.format == OutputFormat::Ids) {
    for (const auto& e : entries) {
      std::cout << e.id << "\n";
    }
    return 0;
  }

  if (opts.format == OutputFormat::Csv) {
    std::cout << "id,mal_id,type,title,status,score,progress,total_units,start_date,finish_date\n";
    for (const auto& e : entries) {
      std::cout << e.id << ","
                << e.malId << ","
                << (e.type == MediaType::Anime ? "anime" : "manga") << ","
                << escapeCsv(e.title) << ","
                << escapeCsv(statusToString(e.status, e.type)) << ","
                << e.score << ","
                << e.progress << ","
                << e.totalUnits << ","
                << escapeCsv(e.startDate) << ","
                << escapeCsv(e.finishDate) << "\n";
    }
    return 0;
  }

  bool tty = IS_TTY();
  bool color = shouldColor();

  int maxIdLen = 4;
  for (const auto& e : entries) {
    maxIdLen = std::max(maxIdLen, static_cast<int>(std::to_string(e.id).size()));
  }

  std::ostringstream out;
  if (tty && color) out << ansi::Bold << ansi::Mauve;
  out << std::right << std::setw(maxIdLen + 2) << "ID" << "  "
      << std::left << std::setw(30) << "TITLE" << "  "
      << std::setw(7) << "TYPE" << "  "
      << std::setw(12) << "STATUS" << "  "
      << std::right << std::setw(7) << "RATING" << "  "
      << std::setw(9) << "PROGRESS";
  if (tty && color) out << ansi::Reset;
  out << "\n";

  if (tty) {
    if (color) out << ansi::Dim << ansi::Subtext;
    out << std::string(maxIdLen + 2, '-') << "  "
        << std::string(30, '-') << "  "
        << std::string(7, '-') << "  "
        << std::string(12, '-') << "  "
        << std::string(7, '-') << "  "
        << std::string(9, '-');
    if (color) out << ansi::Reset;
    out << "\n";
  }

  for (const auto& e : entries) {
    std::string titleDisplay = e.title;
    if (titleDisplay.size() > 28) {
      titleDisplay = titleDisplay.substr(0, 25) + "...";
    }

    std::string scoreStr = (e.score > 0) ? (std::to_string(e.score) + "/10") : "-";
    std::string progStr = std::to_string(e.progress) + "/" + (e.totalUnits > 0 ? std::to_string(e.totalUnits) : "?");

    out << std::right << std::setw(maxIdLen + 2) << e.id << "  "
        << std::left << std::setw(30) << titleDisplay << "  "
        << std::setw(7) << (e.type == MediaType::Anime ? "anime" : "manga") << "  "
        << std::setw(12) << statusToString(e.status, e.type) << "  "
        << std::right << std::setw(7) << scoreStr << "  "
        << std::setw(9) << progStr << "\n";
  }

  if (tty) {
    out << "\n";
    if (color) out << ansi::Subtext;
    out << entries.size() << " entries shown\n";
    if (color) out << ansi::Reset;
  }

  std::cout << out.str();
  return 0;
}

int CliDispatcher::executeCount(const std::vector<std::string>& args, Database& db) {
  CliFilterOptions opts;
  bool jsonMode = false;
  std::string err;

  if (!parseCommonFilters(args, opts, jsonMode, err)) {
    emitError("usage", err, jsonMode);
    return 2;
  }

  auto entries = collectAndFilterEntries(db, opts);
  if (jsonMode) {
    std::cout << "{\"count\":" << entries.size() << "}\n";
  } else {
    std::cout << entries.size() << "\n";
  }
  return 0;
}

static void printSingleDetail(const Entry& e, bool jsonMode) {
  if (jsonMode) {
    std::cout << CliDispatcher::formatJsonEntry(e) << "\n";
    return;
  }

  bool color = shouldColor();
  auto tag = [&](const std::string& name) {
    if (color) std::cout << ansi::Bold << ansi::Subtext << std::left << std::setw(14) << name << ansi::Reset;
    else std::cout << std::left << std::setw(14) << name;
  };

  tag("Title:");      std::cout << (color ? ansi::Bold : "") << e.title << (color ? ansi::Reset : "") << "\n";
  tag("Type:");       std::cout << (e.type == MediaType::Anime ? "Anime" : "Manga") << "\n";
  tag("Status:");     std::cout << statusToString(e.status, e.type) << "\n";
  tag("Rating:");     std::cout << (e.score > 0 ? (std::to_string(e.score) + "/10") : "Unrated") << "\n";
  tag("Progress:");   std::cout << e.progress << " / " << (e.totalUnits > 0 ? std::to_string(e.totalUnits) : "?") << " units\n";
  if (e.type == MediaType::Manga && (e.volumes > 0 || e.totalVolumes > 0)) {
    tag("Volumes:");  std::cout << e.volumes << " / " << (e.totalVolumes > 0 ? std::to_string(e.totalVolumes) : "?") << "\n";
  }
  tag("Dates:");      std::cout << (e.startDate.empty() ? "-" : e.startDate) << " -> " << (e.finishDate.empty() ? "-" : e.finishDate) << "\n";
  tag("MAL ID:");     std::cout << (e.malId > 0 ? std::to_string(e.malId) : "Local") << "\n";
  tag("Local ID:");   std::cout << e.id << "\n";
  tag("Notes:");      std::cout << (e.notes.empty() ? "-" : e.notes) << "\n";
}

int CliDispatcher::executeShow(const std::vector<std::string>& args, Database& db) {
  if (args.empty()) {
    emitError("usage", "missing entry ID or title for 'catalist show <id|title|->'.", false);
    return 2;
  }

  bool jsonMode = false;
  std::string target;

  for (const auto& a : args) {
    if (a == "--json") jsonMode = true;
    else if (target.empty()) target = a;
  }

  if (target.empty()) {
    emitError("usage", "missing target identifier or title.", jsonMode);
    return 2;
  }

  std::vector<std::string> queries;
  if (target == "-") {
    std::string line;
    while (std::getline(std::cin, line)) {
      line.erase(0, line.find_first_not_of(" \t\r\n"));
      line.erase(line.find_last_not_of(" \t\r\n") + 1);
      if (!line.empty()) queries.push_back(line);
    }
  } else {
    queries.push_back(target);
  }

  if (queries.empty()) {
    emitError("input", "no queries received from stdin.", jsonMode);
    return 1;
  }

  int exitCode = 0;
  for (size_t qIdx = 0; qIdx < queries.size(); ++qIdx) {
    const auto& q = queries[qIdx];
    std::optional<Entry> match;

    bool isInt = !q.empty() && std::all_of(q.begin(), q.end(), ::isdigit);
    if (isInt) {
      int id = std::stoi(q);
      match = db.getById(MediaType::Anime, id);
      if (!match.has_value()) {
        match = db.getById(MediaType::Manga, id);
      }
    }

    if (!match.has_value()) {
      std::string qLower = q;
      std::transform(qLower.begin(), qLower.end(), qLower.begin(), ::tolower);

      std::vector<Entry> candidates;
      auto searchType = [&](MediaType t) {
        for (const auto& e : db.getEntries(t)) {
          std::string titleLower = e.title;
          std::transform(titleLower.begin(), titleLower.end(), titleLower.begin(), ::tolower);
          if (titleLower == qLower) {
            candidates.clear();
            candidates.push_back(e);
            return true;
          }
          if (titleLower.find(qLower) != std::string::npos) {
            candidates.push_back(e);
          }
        }
        return false;
      };

      if (!searchType(MediaType::Anime)) {
        searchType(MediaType::Manga);
      }

      if (candidates.size() == 1) {
        match = candidates[0];
      } else if (candidates.size() > 1) {
        std::string msg = "ambiguous query '" + q + "'. Matches multiple entries:\n";
        for (const auto& c : candidates) {
          msg += "  - [" + std::to_string(c.id) + "] " + c.title + " (" + (c.type == MediaType::Anime ? "anime" : "manga") + ")\n";
        }
        emitError("ambiguous", msg, jsonMode);
        exitCode = 1;
        continue;
      }
    }

    if (!match.has_value()) {
      emitError("notFound", "entry not found matching '" + q + "'.", jsonMode);
      exitCode = 1;
      continue;
    }

    printSingleDetail(match.value(), jsonMode);
    if (qIdx + 1 < queries.size()) std::cout << "\n";
  }

  return exitCode;
}

int CliDispatcher::executeAdd(const std::vector<std::string>& args, Database& db) {
  if (args.empty()) {
    emitError("usage", "usage: catalist add <title> [flags]", false);
    return 2;
  }

  std::string title;
  MediaType type = MediaType::Anime;
  WatchStatus status = WatchStatus::Watching;
  int score = 0;
  int progress = 0;
  int totalUnits = 0;
  int volumes = 0;
  std::string startDate;
  std::string finishDate;
  std::string notes;
  bool jsonMode = false;

  for (size_t i = 0; i < args.size(); ++i) {
    const std::string& arg = args[i];

    if (arg == "--json") {
      jsonMode = true;
    } else if (arg == "--type" || arg == "-t") {
      if (++i >= args.size()) { emitError("usage", "missing argument for '--type'", jsonMode); return 2; }
      std::string t = args[i];
      std::transform(t.begin(), t.end(), t.begin(), ::tolower);
      if (t == "anime" || t == "a") type = MediaType::Anime;
      else if (t == "manga" || t == "m") type = MediaType::Manga;
      else { emitError("usage", "invalid type '" + args[i] + "'. Expected 'anime' or 'manga'.", jsonMode); return 2; }
    } else if (arg == "--status" || arg == "-s") {
      if (++i >= args.size()) { emitError("usage", "missing argument for '--status'", jsonMode); return 2; }
      status = stringToStatus(args[i]);
    } else if (arg == "--rating" || arg == "-r" || arg == "--score") {
      if (++i >= args.size()) { emitError("usage", "missing argument for '--rating'", jsonMode); return 2; }
      try { score = std::clamp(std::stoi(args[i]), 0, 10); }
      catch (...) { emitError("usage", "invalid rating '" + args[i] + "'. Expected 0..10.", jsonMode); return 2; }
    } else if (arg == "--progress" || arg == "-p") {
      if (++i >= args.size()) { emitError("usage", "missing argument for '--progress'", jsonMode); return 2; }
      try { progress = std::max(0, std::stoi(args[i])); }
      catch (...) { emitError("usage", "invalid integer for '--progress': " + args[i], jsonMode); return 2; }
    } else if (arg == "--episodes" || arg == "--chapters" || arg == "--total" || arg == "-e") {
      if (++i >= args.size()) { emitError("usage", "missing argument for '--total'", jsonMode); return 2; }
      try { totalUnits = std::max(0, std::stoi(args[i])); }
      catch (...) { emitError("usage", "invalid integer for '--total': " + args[i], jsonMode); return 2; }
    } else if (arg == "--volumes" || arg == "-v") {
      if (++i >= args.size()) { emitError("usage", "missing argument for '--volumes'", jsonMode); return 2; }
      try { volumes = std::max(0, std::stoi(args[i])); }
      catch (...) { emitError("usage", "invalid integer for '--volumes': " + args[i], jsonMode); return 2; }
    } else if (arg == "--start" || arg == "--started") {
      if (++i >= args.size()) { emitError("usage", "missing date for '--start'", jsonMode); return 2; }
      startDate = args[i];
    } else if (arg == "--finish" || arg == "--finished") {
      if (++i >= args.size()) { emitError("usage", "missing date for '--finish'", jsonMode); return 2; }
      finishDate = args[i];
    } else if (arg == "--notes" || arg == "-n") {
      if (++i >= args.size()) { emitError("usage", "missing text for '--notes'", jsonMode); return 2; }
      notes = args[i];
    } else if (title.empty() && arg.rfind("-", 0) != 0) {
      title = arg;
    } else {
      emitError("usage", "unknown option or unexpected argument: '" + arg + "'", jsonMode);
      return 2;
    }
  }

  if (title.empty()) {
    emitError("usage", "missing title for 'catalist add <title> [flags]'.", jsonMode);
    return 2;
  }

  if (startDate.empty()) {
    auto now = std::chrono::system_clock::now();
    auto inTimeT = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&inTimeT), "%Y-%m-%d");
    startDate = ss.str();
  }

  Entry e;
  e.type = type;
  e.title = title;
  e.status = status;
  e.score = score;
  e.progress = progress;
  e.totalUnits = totalUnits;
  e.volumes = volumes;
  e.startDate = startDate;
  e.finishDate = finishDate;
  e.notes = notes;
  e.updatedAt = startDate;

  int assignedId = db.addEntry(e);
  db.saveAll();

  if (jsonMode) {
    auto opt = db.getById(type, assignedId);
    if (opt.has_value()) {
      std::cout << formatJsonEntry(opt.value()) << "\n";
    }
  } else {
    bool color = shouldColor();
    std::cout << (color ? ansi::Green + ansi::Bold : "") << "Added "
              << (type == MediaType::Anime ? "anime" : "manga")
              << " '" << title << "' [ID " << assignedId << "]"
              << (color ? ansi::Reset : "") << "\n";
  }

  return 0;
}

int CliDispatcher::executeSet(const std::vector<std::string>& args, Database& db) {
  if (args.empty()) {
    emitError("usage", "usage: catalist set <id|title|-> [flags]", false);
    return 2;
  }

  std::string target;
  bool jsonMode = false;
  std::optional<int> setProgress;
  int deltaProgress = 0;
  bool hasDelta = false;
  std::optional<int> setScore;
  std::optional<WatchStatus> setStatus;
  std::optional<int> setTotal;
  std::optional<int> setVolumes;
  std::optional<std::string> setStart;
  std::optional<std::string> setFinish;
  std::optional<std::string> setNotes;
  std::optional<std::string> setTitle;

  for (size_t i = 0; i < args.size(); ++i) {
    const std::string& arg = args[i];

    if (arg == "--json") {
      jsonMode = true;
    } else if (arg == "--progress" || arg == "-p") {
      if (++i >= args.size()) { emitError("usage", "missing value for '--progress'", jsonMode); return 2; }
      std::string val = args[i];
      if (val == "add" || val == "a") {
        int delta = 1;
        if (i + 1 < args.size()) {
          const std::string& next = args[i + 1];
          if (!next.empty() && std::all_of(next.begin(), next.end(), ::isdigit)) {
            delta = std::stoi(next);
            ++i;
          }
        }
        deltaProgress = delta;
        hasDelta = true;
      } else if (val == "sub" || val == "s" || val == "dec") {
        int delta = 1;
        if (i + 1 < args.size()) {
          const std::string& next = args[i + 1];
          if (!next.empty() && std::all_of(next.begin(), next.end(), ::isdigit)) {
            delta = std::stoi(next);
            ++i;
          }
        }
        deltaProgress = -delta;
        hasDelta = true;
      } else if (val.front() == '+' || val.front() == '-') {
        try {
          deltaProgress = std::stoi(val);
          hasDelta = true;
        } catch (...) { emitError("usage", "invalid delta for '--progress': " + val, jsonMode); return 2; }
      } else {
        try {
          setProgress = std::max(0, std::stoi(val));
        } catch (...) { emitError("usage", "invalid integer for '--progress': " + val, jsonMode); return 2; }
      }
    } else if (arg == "--rating" || arg == "-r" || arg == "--score") {
      if (++i >= args.size()) { emitError("usage", "missing argument for '--rating'", jsonMode); return 2; }
      try { setScore = std::clamp(std::stoi(args[i]), 0, 10); }
      catch (...) { emitError("usage", "invalid score for '--rating': " + args[i], jsonMode); return 2; }
    } else if (arg == "--status" || arg == "-s") {
      if (++i >= args.size()) { emitError("usage", "missing argument for '--status'", jsonMode); return 2; }
      setStatus = stringToStatus(args[i]);
    } else if (arg == "--episodes" || arg == "--chapters" || arg == "--total" || arg == "-e") {
      if (++i >= args.size()) { emitError("usage", "missing argument for '--total'", jsonMode); return 2; }
      try { setTotal = std::max(0, std::stoi(args[i])); }
      catch (...) { emitError("usage", "invalid integer for '--total': " + args[i], jsonMode); return 2; }
    } else if (arg == "--volumes" || arg == "-v") {
      if (++i >= args.size()) { emitError("usage", "missing argument for '--volumes'", jsonMode); return 2; }
      try { setVolumes = std::max(0, std::stoi(args[i])); }
      catch (...) { emitError("usage", "invalid integer for '--volumes': " + args[i], jsonMode); return 2; }
    } else if (arg == "--start" || arg == "--started") {
      if (++i >= args.size()) { emitError("usage", "missing date for '--start'", jsonMode); return 2; }
      setStart = args[i];
    } else if (arg == "--finish" || arg == "--finished") {
      if (++i >= args.size()) { emitError("usage", "missing date for '--finish'", jsonMode); return 2; }
      setFinish = args[i];
    } else if (arg == "--notes" || arg == "-n") {
      if (++i >= args.size()) { emitError("usage", "missing text for '--notes'", jsonMode); return 2; }
      setNotes = args[i];
    } else if (arg == "--title") {
      if (++i >= args.size()) { emitError("usage", "missing text for '--title'", jsonMode); return 2; }
      setTitle = args[i];
    } else if (target.empty() && arg.rfind("-", 0) != 0) {
      target = arg;
    } else if (target.empty() && arg == "-") {
      target = "-";
    } else {
      emitError("usage", "unknown option or unexpected argument: '" + arg + "'", jsonMode);
      return 2;
    }
  }

  if (target.empty()) {
    emitError("usage", "missing entry ID or title for 'catalist set <id|title|-> [flags]'.", jsonMode);
    return 2;
  }

  std::vector<std::string> queries;
  if (target == "-") {
    std::string line;
    while (std::getline(std::cin, line)) {
      line.erase(0, line.find_first_not_of(" \t\r\n"));
      line.erase(line.find_last_not_of(" \t\r\n") + 1);
      if (!line.empty()) queries.push_back(line);
    }
  } else {
    queries.push_back(target);
  }

  if (queries.empty()) {
    emitError("input", "no target identifiers received from stdin.", jsonMode);
    return 1;
  }

  int successCount = 0;
  for (const auto& q : queries) {
    std::optional<Entry> match;

    bool isInt = !q.empty() && std::all_of(q.begin(), q.end(), ::isdigit);
    if (isInt) {
      int id = std::stoi(q);
      match = db.getById(MediaType::Anime, id);
      if (!match.has_value()) {
        match = db.getById(MediaType::Manga, id);
      }
    }

    if (!match.has_value()) {
      std::string qLower = q;
      std::transform(qLower.begin(), qLower.end(), qLower.begin(), ::tolower);

      std::vector<Entry> candidates;
      auto searchType = [&](MediaType t) {
        for (const auto& e : db.getEntries(t)) {
          std::string titleLower = e.title;
          std::transform(titleLower.begin(), titleLower.end(), titleLower.begin(), ::tolower);
          if (titleLower == qLower) {
            candidates.clear();
            candidates.push_back(e);
            return true;
          }
          if (titleLower.find(qLower) != std::string::npos) {
            candidates.push_back(e);
          }
        }
        return false;
      };

      if (!searchType(MediaType::Anime)) {
        searchType(MediaType::Manga);
      }

      if (candidates.size() == 1) {
        match = candidates[0];
      } else if (candidates.size() > 1) {
        emitError("ambiguous", "ambiguous query '" + q + "'. Matches multiple entries.", jsonMode);
        continue;
      }
    }

    if (!match.has_value()) {
      emitError("notFound", "entry not found matching '" + q + "'.", jsonMode);
      continue;
    }

    auto e = match.value();

    if (setProgress.has_value()) {
      e.progress = setProgress.value();
    } else if (hasDelta) {
      e.progress = std::max(0, e.progress + deltaProgress);
    }

    if (e.totalUnits > 0 && e.progress >= e.totalUnits) {
      e.progress = e.totalUnits;
      e.status = WatchStatus::Completed;
    }

    if (setScore.has_value()) e.score = setScore.value();
    if (setStatus.has_value()) e.status = setStatus.value();
    if (setTotal.has_value()) e.totalUnits = setTotal.value();
    if (setVolumes.has_value()) e.volumes = setVolumes.value();
    if (setStart.has_value()) e.startDate = setStart.value();
    if (setFinish.has_value()) e.finishDate = setFinish.value();
    if (setNotes.has_value()) e.notes = setNotes.value();
    if (setTitle.has_value()) e.title = setTitle.value();

    auto now = std::chrono::system_clock::now();
    auto inTimeT = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&inTimeT), "%Y-%m-%d");
    e.updatedAt = ss.str();

    db.updateEntry(e);
    successCount++;

    if (jsonMode) {
      std::cout << formatJsonEntry(e) << "\n";
    } else {
      bool color = shouldColor();
      std::cout << (color ? ansi::Green : "") << "Updated [" << e.id << "] '" << e.title << "': "
                << "Progress " << e.progress << "/" << (e.totalUnits > 0 ? std::to_string(e.totalUnits) : "?") << ", "
                << "Rating " << e.score << "/10, "
                << "Status " << statusToString(e.status, e.type)
                << (color ? ansi::Reset : "") << "\n";
    }
  }

  if (successCount > 0) {
    db.saveAll();
    return 0;
  }
  return 1;
}

int CliDispatcher::executeRm(const std::vector<std::string>& args, Database& db) {
  if (args.empty()) {
    emitError("usage", "usage: catalist rm <id|title|-> [--yes]", false);
    return 2;
  }

  std::string target;
  bool forceYes = false;
  bool jsonMode = false;

  for (const auto& a : args) {
    if (a == "--yes" || a == "-y") forceYes = true;
    else if (a == "--json") jsonMode = true;
    else if (target.empty()) target = a;
  }

  if (target.empty()) {
    emitError("usage", "missing target identifier or title for 'catalist rm'.", jsonMode);
    return 2;
  }

  std::vector<std::string> queries;
  if (target == "-") {
    std::string line;
    while (std::getline(std::cin, line)) {
      line.erase(0, line.find_first_not_of(" \t\r\n"));
      line.erase(line.find_last_not_of(" \t\r\n") + 1);
      if (!line.empty()) queries.push_back(line);
    }
  } else {
    queries.push_back(target);
  }

  if (queries.empty()) {
    emitError("input", "no targets received from stdin.", jsonMode);
    return 1;
  }

  int deletedCount = 0;
  for (const auto& q : queries) {
    std::optional<Entry> match;

    bool isInt = !q.empty() && std::all_of(q.begin(), q.end(), ::isdigit);
    if (isInt) {
      int id = std::stoi(q);
      match = db.getById(MediaType::Anime, id);
      if (!match.has_value()) {
        match = db.getById(MediaType::Manga, id);
      }
    }

    if (!match.has_value()) {
      std::string qLower = q;
      std::transform(qLower.begin(), qLower.end(), qLower.begin(), ::tolower);

      std::vector<Entry> candidates;
      auto searchType = [&](MediaType t) {
        for (const auto& e : db.getEntries(t)) {
          std::string titleLower = e.title;
          std::transform(titleLower.begin(), titleLower.end(), titleLower.begin(), ::tolower);
          if (titleLower == qLower) {
            candidates.clear();
            candidates.push_back(e);
            return true;
          }
          if (titleLower.find(qLower) != std::string::npos) {
            candidates.push_back(e);
          }
        }
        return false;
      };

      if (!searchType(MediaType::Anime)) {
        searchType(MediaType::Manga);
      }

      if (candidates.size() == 1) {
        match = candidates[0];
      } else if (candidates.size() > 1) {
        emitError("ambiguous", "ambiguous query '" + q + "'. Matches multiple entries.", jsonMode);
        continue;
      }
    }

    if (!match.has_value()) {
      emitError("notFound", "entry not found matching '" + q + "'.", jsonMode);
      continue;
    }

    const auto& e = match.value();

    if (!forceYes) {
      if (!IS_TTY()) {
        emitError("safety", "deletion of '" + e.title + "' requires '--yes' when running in non-interactive pipeline.", jsonMode);
        return 1;
      }
      std::cout << "Delete '" << e.title << "' [ID " << e.id << "]? [y/N]: " << std::flush;
      std::string resp;
      if (!std::getline(std::cin, resp) || (resp != "y" && resp != "Y")) {
        std::cout << "Canceled.\n";
        continue;
      }
    }

    db.removeEntry(e.type, e.id);
    deletedCount++;

    if (jsonMode) {
      std::cout << "{\"deleted\":true,\"id\":" << e.id << ",\"title\":\"" << escapeJsonString(e.title) << "\"}\n";
    } else {
      bool color = shouldColor();
      std::cout << (color ? ansi::Red : "") << "Deleted '" << e.title << "' [ID " << e.id << "]"
                << (color ? ansi::Reset : "") << "\n";
    }
  }

  if (deletedCount > 0) {
    db.saveAll();
    return 0;
  }
  return 1;
}

int CliDispatcher::executeRandom(const std::vector<std::string>& args, Database& db) {
  CliFilterOptions opts;
  bool jsonMode = false;
  std::string err;

  if (!parseCommonFilters(args, opts, jsonMode, err)) {
    emitError("usage", err, jsonMode);
    return 2;
  }

  auto entries = collectAndFilterEntries(db, opts);
  if (entries.empty()) {
    emitError("notFound", "no entries match the specified filters.", jsonMode);
    return 1;
  }

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<size_t> dist(0, entries.size() - 1);
  const auto& picked = entries[dist(gen)];

  if (opts.format == OutputFormat::Plain) {
    std::cout << picked.title << "\n";
    return 0;
  }

  if (opts.format == OutputFormat::Ids) {
    std::cout << picked.id << "\n";
    return 0;
  }

  if (opts.format == OutputFormat::Json) {
    std::cout << formatJsonEntry(picked) << "\n";
    return 0;
  }

  printSingleDetail(picked, false);
  return 0;
}

int CliDispatcher::executeStats(const std::vector<std::string>& args, const Database& db) {
  bool jsonMode = false;
  std::optional<MediaType> filterType;

  for (size_t i = 0; i < args.size(); ++i) {
    if (args[i] == "--json") jsonMode = true;
    else if (args[i] == "--type" || args[i] == "-t") {
      if (++i < args.size()) {
        std::string t = args[i];
        std::transform(t.begin(), t.end(), t.begin(), ::tolower);
        if (t == "anime" || t == "a") filterType = MediaType::Anime;
        else if (t == "manga" || t == "m") filterType = MediaType::Manga;
      }
    }
  }

  auto getStatsObj = [&](MediaType type) {
    std::ostringstream ss;
    ss << "{"
       << "\"total\":" << db.getEntries(type).size() << ","
       << "\"daysSpent\":" << std::fixed << std::setprecision(1) << db.daysSpent(type) << ","
       << "\"meanScore\":" << db.averageScore(type) << ","
       << "\"completedPercent\":" << db.completedPercentage(type) << ","
       << "\"progressUnits\":" << db.totalProgress(type) << ","
       << "\"rewatched\":" << db.totalRewatched(type) << ","
       << "\"watching\":" << db.countByStatus(type, WatchStatus::Watching) << ","
       << "\"completed\":" << db.countByStatus(type, WatchStatus::Completed) << ","
       << "\"onHold\":" << db.countByStatus(type, WatchStatus::OnHold) << ","
       << "\"dropped\":" << db.countByStatus(type, WatchStatus::Dropped) << ","
       << "\"planned\":" << db.countByStatus(type, WatchStatus::PlanToWatch);
    if (type == MediaType::Manga) {
      ss << ",\"volumes\":" << db.totalVolumes(MediaType::Manga);
    }
    ss << "}";
    return ss.str();
  };

  if (jsonMode) {
    if (filterType.has_value()) {
      std::cout << getStatsObj(filterType.value()) << "\n";
    } else {
      std::cout << "{\"anime\":" << getStatsObj(MediaType::Anime)
                << ",\"manga\":" << getStatsObj(MediaType::Manga) << "}\n";
    }
    return 0;
  }

  bool color = shouldColor();

  auto printMediaStats = [&](MediaType type) {
    std::string name = (type == MediaType::Anime ? "Anime" : "Manga");
    size_t tot = db.getEntries(type).size();
    size_t cWatch = db.countByStatus(type, WatchStatus::Watching);
    size_t cComp = db.countByStatus(type, WatchStatus::Completed);
    size_t cHold = db.countByStatus(type, WatchStatus::OnHold);
    size_t cDrop = db.countByStatus(type, WatchStatus::Dropped);
    size_t cPlan = db.countByStatus(type, WatchStatus::PlanToWatch);

    if (color) std::cout << ansi::Bold << (type == MediaType::Anime ? ansi::Lavender : ansi::Peach) << "=== " << name << " Telemetry ===" << ansi::Reset << "\n";
    else std::cout << "=== " << name << " Telemetry ===\n";

    std::cout << "  Total Records:      " << tot << "\n";
    std::cout << "  Days Logged:        " << std::fixed << std::setprecision(1) << db.daysSpent(type) << " days\n";
    std::cout << "  Mean Rating:        " << std::fixed << std::setprecision(2) << db.averageScore(type) << " / 10\n";
    std::cout << "  Completed:          " << std::fixed << std::setprecision(1) << db.completedPercentage(type) << "%\n";
    std::cout << "  " << (type == MediaType::Anime ? "Episodes" : "Chapters") << " Logged:   " << db.totalProgress(type) << "\n";
    if (type == MediaType::Manga) {
      std::cout << "  Volumes Logged:     " << db.totalVolumes(MediaType::Manga) << "\n";
    }
    std::cout << "  Status Breakdown:   "
              << (color ? ansi::Green : "") << cWatch << " " << (type == MediaType::Anime ? "watching" : "reading") << ", "
              << (color ? ansi::Blue : "") << cComp << " completed, "
              << (color ? ansi::Yellow : "") << cHold << " on-hold, "
              << (color ? ansi::Red : "") << cDrop << " dropped, "
              << (color ? ansi::Subtext : "") << cPlan << " planned"
              << (color ? ansi::Reset : "") << "\n\n";
  };

  if (!filterType.has_value() || filterType.value() == MediaType::Anime) {
    printMediaStats(MediaType::Anime);
  }
  if (!filterType.has_value() || filterType.value() == MediaType::Manga) {
    printMediaStats(MediaType::Manga);
  }

  return 0;
}

int CliDispatcher::executeExport(const std::vector<std::string>& args, const Database& db) {
  if (args.empty()) {
    emitError("usage", "usage: catalist export <json|csv> [--type <anime|manga>]", false);
    return 2;
  }

  std::string formatStr = args[0];
  std::transform(formatStr.begin(), formatStr.end(), formatStr.begin(), ::tolower);

  std::optional<MediaType> filterType;
  for (size_t i = 1; i < args.size(); ++i) {
    if (args[i] == "--type" || args[i] == "-t") {
      if (++i < args.size()) {
        std::string t = args[i];
        std::transform(t.begin(), t.end(), t.begin(), ::tolower);
        if (t == "anime" || t == "a") filterType = MediaType::Anime;
        else if (t == "manga" || t == "m") filterType = MediaType::Manga;
      }
    }
  }

  if (formatStr == "json") {
    if (filterType.has_value()) {
      std::cout << formatJsonEntries(db.getEntries(filterType.value()));
    } else {
      std::cout << "{\n  \"anime\": " << formatJsonEntries(db.getEntries(MediaType::Anime))
                << ",\n  \"manga\": " << formatJsonEntries(db.getEntries(MediaType::Manga))
                << "\n}\n";
    }
    return 0;
  }

  if (formatStr == "csv") {
    std::cout << "#id,mal_id,anilist_id,type,title,status,score,progress,total_units,volumes,total_volumes,rewatched,start_date,finish_date,notes,updated_at\n";
    auto dumpList = [](const std::vector<Entry>& list) {
      for (const auto& e : list) {
        std::cout << e.id << ","
                  << e.malId << ","
                  << e.anilistId << ","
                  << (e.type == MediaType::Anime ? "anime" : "manga") << ","
                  << escapeCsv(e.title) << ","
                  << escapeCsv(statusToString(e.status, e.type)) << ","
                  << e.score << ","
                  << e.progress << ","
                  << e.totalUnits << ","
                  << e.volumes << ","
                  << e.totalVolumes << ","
                  << e.rewatched << ","
                  << escapeCsv(e.startDate) << ","
                  << escapeCsv(e.finishDate) << ","
                  << escapeCsv(e.notes) << ","
                  << escapeCsv(e.updatedAt) << "\n";
      }
    };

    if (!filterType.has_value() || filterType.value() == MediaType::Anime) {
      dumpList(db.getEntries(MediaType::Anime));
    }
    if (!filterType.has_value() || filterType.value() == MediaType::Manga) {
      dumpList(db.getEntries(MediaType::Manga));
    }
    return 0;
  }

  emitError("usage", "unsupported export format '" + formatStr + "'. Expected 'json' or 'csv'.", false);
  return 2;
}

int CliDispatcher::executeImport(const std::vector<std::string>& args, Database& db) {
  if (args.empty()) {
    emitError("usage", "usage: catalist import [--clean] <file.xml ...>", false);
    return 2;
  }

  bool clean = false;
  std::vector<std::string> files;

  for (const auto& a : args) {
    if (a == "--clean" || a == "--wipe") clean = true;
    else files.push_back(a);
  }

  if (files.empty()) {
    emitError("usage", "no XML files specified for import.", false);
    return 2;
  }

  size_t totalImported = 0;
  for (const auto& f : files) {
    size_t count = MalParser::importXml(f, db, clean);
    std::cout << "Successfully ingested " << count << " records from: " << f << "\n";
    totalImported += count;
  }

  std::cout << "Import complete. " << totalImported << " total records loaded.\n";
  return 0;
}

int CliDispatcher::executeSearch(const std::vector<std::string>& args, Database& db) {
  if (args.empty()) {
    emitError("usage", "usage: catalist search <query> [--type <anime|manga>] [--limit <N>]", false);
    return 2;
  }

  std::string queryStr;
  MediaType type = MediaType::Anime;
  size_t limit = 10;
  OutputFormat format = OutputFormat::Table;
  bool jsonMode = false;

  for (size_t i = 0; i < args.size(); ++i) {
    if (args[i] == "--json") {
      format = OutputFormat::Json;
      jsonMode = true;
    } else if (args[i] == "--plain") {
      format = OutputFormat::Plain;
    } else if (args[i] == "--type" || args[i] == "-t") {
      if (++i < args.size()) {
        std::string t = args[i];
        std::transform(t.begin(), t.end(), t.begin(), ::tolower);
        if (t == "anime" || t == "a") type = MediaType::Anime;
        else if (t == "manga" || t == "m") type = MediaType::Manga;
      }
    } else if (args[i] == "--limit" || args[i] == "-l") {
      if (++i < args.size()) {
        try { limit = std::clamp(std::stoi(args[i]), 1, 25); } catch (...) {}
      }
    } else if (queryStr.empty() && args[i].rfind("-", 0) != 0) {
      queryStr = args[i];
    }
  }

  if (queryStr.empty()) {
    emitError("usage", "missing search query for 'catalist search <query>'.", jsonMode);
    return 2;
  }

  std::string err;
  auto results = RemoteSearchEngine::query(type, queryStr, err, limit);

  if (results.empty()) {
    emitError("notFound", err.empty() ? ("no results found for '" + queryStr + "'.") : err, jsonMode);
    return 1;
  }

  if (format == OutputFormat::Plain) {
    for (const auto& r : results) std::cout << r.title << "\n";
    return 0;
  }

  if (format == OutputFormat::Json) {
    std::cout << "[\n";
    for (size_t i = 0; i < results.size(); ++i) {
      const auto& r = results[i];
      std::cout << "  {"
                << "\"malId\":" << r.malId << ","
                << "\"title\":\"" << escapeJsonString(r.title) << "\","
                << "\"type\":\"" << (r.type == MediaType::Anime ? "anime" : "manga") << "\","
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
  if (color) std::cout << ansi::Bold << ansi::Mauve;
  std::cout << "  #  " << std::left << std::setw(38) << "TITLE" << "  "
            << std::setw(7) << "TYPE" << "  "
            << std::setw(9) << "EPISODES" << "  "
            << std::setw(6) << "YEAR" << "  "
            << std::setw(6) << "SCORE" << "  "
            << "MAL ID\n";
  if (color) std::cout << ansi::Reset << ansi::Dim << ansi::Subtext;
  std::cout << " --  " << std::string(38, '-') << "  "
            << std::string(7, '-') << "  "
            << std::string(9, '-') << "  "
            << std::string(6, '-') << "  "
            << std::string(6, '-') << "  "
            << "------\n";
  if (color) std::cout << ansi::Reset;

  for (size_t i = 0; i < results.size(); ++i) {
    const auto& r = results[i];
    std::string tDisplay = r.title;
    if (tDisplay.size() > 36) tDisplay = tDisplay.substr(0, 33) + "...";
    std::string epStr = (r.totalUnits > 0 ? (std::to_string(r.totalUnits) + " eps") : "ongoing");

    std::cout << " " << std::right << std::setw(2) << (i + 1) << "  "
              << std::left << std::setw(38) << tDisplay << "  "
              << std::setw(7) << (r.type == MediaType::Anime ? "anime" : "manga") << "  "
              << std::setw(9) << epStr << "  "
              << std::setw(6) << (r.year.empty() ? "-" : r.year) << "  "
              << std::setw(6) << std::fixed << std::setprecision(2) << r.score << "  "
              << r.malId << "\n";
  }

  if (!IS_TTY()) return 0;

  std::cout << "\nSelect number to add [1-" << results.size() << ", or 'q' to quit]: " << std::flush;
  std::string choice;
  if (!std::getline(std::cin, choice) || choice == "q" || choice == "Q" || choice.empty()) {
    std::cout << "Canceled.\n";
    return 0;
  }

  int pick = 0;
  try { pick = std::stoi(choice); } catch (...) { return 0; }
  if (pick < 1 || pick > static_cast<int>(results.size())) {
    std::cout << "Invalid selection.\n";
    return 1;
  }

  const auto& selected = results[pick - 1];

  std::cout << "Select status [1: Watching, 2: Completed, 3: Plan to Watch] (default: 1): " << std::flush;
  std::string stChoice;
  WatchStatus st = WatchStatus::Watching;
  if (std::getline(std::cin, stChoice) && !stChoice.empty()) {
    if (stChoice == "2") st = WatchStatus::Completed;
    else if (stChoice == "3") st = WatchStatus::PlanToWatch;
  }

  Entry e;
  e.id = selected.malId;
  e.malId = selected.malId;
  e.type = selected.type;
  e.title = selected.title;
  e.status = st;
  e.score = 0;
  e.progress = (st == WatchStatus::Completed) ? selected.totalUnits : 0;
  e.totalUnits = selected.totalUnits;
  e.startDate = selected.year.empty() ? "0000-00-00" : (selected.year + "-01-01");

  db.addEntry(e);
  db.saveAll();

  if (color) std::cout << ansi::Green << ansi::Bold;
  std::cout << "Added " << (e.type == MediaType::Anime ? "anime" : "manga")
            << " '" << e.title << "' [MAL ID " << e.malId << "] to watchlist.\n";
  if (color) std::cout << ansi::Reset;

  return 0;
}

int CliDispatcher::executeConfig(const std::vector<std::string>& args, AppConfig& config) {
  auto configPath = Database::getConfigDir() / "config.toml";

  if (args.empty() || args[0] == "list") {
    std::cout << "mouse = " << (config.mouse ? "true" : "false") << "\n"
              << "theme = \"" << config.theme << "\"\n"
              << "default_media = \"" << config.defaultMedia << "\"\n"
              << "default_sort = \"" << config.defaultSort << "\"\n"
              << "reverse_sort = " << (config.reverseSort ? "true" : "false") << "\n";
    return 0;
  }

  std::string action = args[0];

  if (action == "get") {
    if (args.size() < 2) {
      emitError("usage", "usage: catalist config get <key>", false);
      return 2;
    }
    std::string key = args[1];
    if (key == "mouse") std::cout << (config.mouse ? "true" : "false") << "\n";
    else if (key == "theme") std::cout << config.theme << "\n";
    else if (key == "default_media" || key == "defaultMedia") std::cout << config.defaultMedia << "\n";
    else if (key == "default_sort" || key == "defaultSort") std::cout << config.defaultSort << "\n";
    else if (key == "reverse_sort" || key == "reverseSort") std::cout << (config.reverseSort ? "true" : "false") << "\n";
    else {
      emitError("config", "unknown config key '" + key + "'.", false);
      return 1;
    }
    return 0;
  }

  if (action == "set") {
    if (args.size() < 3) {
      emitError("usage", "usage: catalist config set <key> <value>", false);
      return 2;
    }
    std::string key = args[1];
    std::string val = args[2];

    if (key == "mouse") config.mouse = (val == "true" || val == "1");
    else if (key == "theme") config.theme = val;
    else if (key == "default_media" || key == "defaultMedia") config.defaultMedia = val;
    else if (key == "default_sort" || key == "defaultSort") config.defaultSort = val;
    else if (key == "reverse_sort" || key == "reverseSort") config.reverseSort = (val == "true" || val == "1");
    else {
      emitError("config", "unknown config key '" + key + "'.", false);
      return 1;
    }

    config.save(configPath);
    std::cout << "Set " << key << " = " << val << "\n";
    return 0;
  }

  emitError("usage", "unknown config action '" + action + "'. Expected: list, get, set.", false);
  return 2;
}

int CliDispatcher::executeCompletion(const std::vector<std::string>& args) {
  if (args.empty()) {
    emitError("usage", "usage: catalist completion <bash|zsh|fish>", false);
    return 2;
  }

  std::string shell = args[0];
  std::transform(shell.begin(), shell.end(), shell.begin(), ::tolower);

  if (shell == "bash") {
    std::cout << R"bash(# Bash completion for catalist
_catalist_completions() {
  local cur prev subcommands
  cur="${COMP_WORDS[COMP_CWORD]}"
  prev="${COMP_WORDS[COMP_CWORD-1]}"
  subcommands="ls show count random search add set rm stats export import config completion"

  if [ $COMP_CWORD -eq 1 ]; then
    COMPREPLY=( $(compgen -W "${subcommands} --version --help -v -h" -- "${cur}") )
    return 0
  fi

  case "${prev}" in
    --type|-t)
      COMPREPLY=( $(compgen -W "anime manga" -- "${cur}") )
      return 0
      ;;
    --status|-s)
      COMPREPLY=( $(compgen -W "watching reading completed on-hold dropped planned" -- "${cur}") )
      return 0
      ;;
    --sort)
      COMPREPLY=( $(compgen -W "date title status rating progress id" -- "${cur}") )
      return 0
      ;;
    --progress|-p)
      COMPREPLY=( $(compgen -W "add a sub s" -- "${cur}") )
      return 0
      ;;
    export)
      COMPREPLY=( $(compgen -W "json csv" -- "${cur}") )
      return 0
      ;;
    config)
      COMPREPLY=( $(compgen -W "list get set" -- "${cur}") )
      return 0
      ;;
    completion)
      COMPREPLY=( $(compgen -W "bash zsh fish" -- "${cur}") )
      return 0
      ;;
  esac

  COMPREPLY=( $(compgen -W "--type --status --rating --unrated --year --search --sort --reverse --limit --plain --ids --csv --json" -- "${cur}") )
}
complete -F _catalist_completions catalist
)bash";
    return 0;
  }

  if (shell == "zsh") {
    std::cout << R"zsh(#compdef catalist

_catalist() {
  local -a commands
  commands=(
    'ls:List watchlist entries'
    'show:Show detailed entry information'
    'count:Count matching records'
    'random:Pick random entry'
    'search:Search MyAnimeList online'
    'add:Add new record'
    'set:Update existing record'
    'rm:Remove entry'
    'stats:Display analytics telemetry'
    'export:Dump database to stdout'
    'import:Ingest MyAnimeList XML export'
    'config:Inspect or modify config.toml'
    'completion:Generate shell autocompletions'
  )

  _arguments -C \
    '1: :->command' \
    '*:: :->args'

  case $state in
    command)
      _describe -t commands 'catalist command' commands
      ;;
    args)
      case $words[1] in
        ls|count|random)
          _arguments \
            '(-t --type)'{-t,--type}'[Media type]:type:(anime manga)' \
            '(-s --status)'{-s,--status}'[Watch status]:status:(watching reading completed on-hold dropped planned)' \
            '(-r --rating)'{-r,--rating}'[Rating spec]:rating:' \
            '--unrated[Unrated entries only]' \
            '(-y --year)'{-y,--year}'[Filter year]:year:' \
            '(-q --search)'{-q,--search}'[Search title]:query:' \
            '--sort[Sort field]:sort:(date title status rating progress id)' \
            '(-R --reverse)'{-R,--reverse}'[Reverse sort]' \
            '(-l --limit)'{-l,--limit}'[Limit count]:limit:' \
            '--plain[Titles only]' \
            '--ids[IDs only]' \
            '--csv[Output CSV]' \
            '--json[Output JSON]'
          ;;
        set)
          _arguments \
            '(-p --progress)'{-p,--progress}'[Progress value or delta]:val:(add a sub s)' \
            '(-r --rating)'{-r,--rating}'[Rating score]:score:' \
            '(-s --status)'{-s,--status}'[Status]:status:(watching reading completed on-hold dropped planned)' \
            '--title[New title]:title:' \
            '-n[Notes]:notes:'
          ;;
        export)
          _arguments '1:format:(json csv)'
          ;;
        config)
          _arguments '1:action:(list get set)'
          ;;
        completion)
          _arguments '1:shell:(bash zsh fish)'
          ;;
      esac
      ;;
  esac
}

_catalist "$@"
)zsh";
    return 0;
  }

  if (shell == "fish") {
    std::cout << R"fish(# Fish completion for catalist
set -l commands ls show count random search add set rm stats export import config completion

complete -c catalist -f
complete -c catalist -n "not __fish_seen_subcommand_from $commands" -a "ls" -d "List entries"
complete -c catalist -n "not __fish_seen_subcommand_from $commands" -a "show" -d "Show entry details"
complete -c catalist -n "not __fish_seen_subcommand_from $commands" -a "count" -d "Count entries"
complete -c catalist -n "not __fish_seen_subcommand_from $commands" -a "random" -d "Sample random entry"
complete -c catalist -n "not __fish_seen_subcommand_from $commands" -a "search" -d "Search MyAnimeList"
complete -c catalist -n "not __fish_seen_subcommand_from $commands" -a "add" -d "Add new record"
complete -c catalist -n "not __fish_seen_subcommand_from $commands" -a "set" -d "Update record"
complete -c catalist -n "not __fish_seen_subcommand_from $commands" -a "rm" -d "Remove entry"
complete -c catalist -n "not __fish_seen_subcommand_from $commands" -a "stats" -d "Analytics telemetry"
complete -c catalist -n "not __fish_seen_subcommand_from $commands" -a "export" -d "Dump database"
complete -c catalist -n "not __fish_seen_subcommand_from $commands" -a "config" -d "Modify configuration"
complete -c catalist -n "not __fish_seen_subcommand_from $commands" -a "completion" -d "Generate shell completion"

complete -c catalist -n "__fish_seen_subcommand_from ls count random" -l type -s t -r -a "anime manga"
complete -c catalist -n "__fish_seen_subcommand_from ls count random" -l status -s s -r -a "watching reading completed on-hold dropped planned"
complete -c catalist -n "__fish_seen_subcommand_from ls count random" -l sort -r -a "date title status rating progress id"
complete -c catalist -n "__fish_seen_subcommand_from export" -a "json csv"
complete -c catalist -n "__fish_seen_subcommand_from completion" -a "bash zsh fish"
complete -c catalist -n "__fish_seen_subcommand_from config" -a "list get set"
)fish";
    return 0;
  }

  emitError("usage", "unsupported shell '" + shell + "'. Supported: bash, zsh, fish.", false);
  return 2;
}

} // namespace catalist
