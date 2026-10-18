#include "catalist/cli.hpp"
#include "catalist/malParser.hpp"
#include "catalist/theme.hpp"

#include <iostream>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <cstdlib>

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
            << "Commands:\n"
            << "  ls                   List watchlist entries (table, plain, or JSON)\n"
            << "  show <id|title|->    Show detailed information for an entry\n"
            << "  count                Count matching entries\n"
            << "  import <file.xml>    Ingest MyAnimeList XML export\n"
            << "  --version, -v        Display version info\n"
            << "  --help, -h           Show this message\n\n"
            << "Listing & Filtering Flags (ls, count):\n"
            << "  --type <anime|manga> Filter by media type\n"
            << "  --status <status>    Filter: watching, reading, completed, on-hold, dropped, planned\n"
            << "  --rating <spec>      Filter rating: exact (9), relational ('>=9', '<=7'), or range ('6..8')\n"
            << "  --unrated            Filter entries without a score (score = 0)\n"
            << "  --year <YYYY>        Filter by start year\n"
            << "  --search <query>     Filter by title substring match\n"
            << "  --sort <field>       Sort by: date, title, status, rating, progress (default: rating)\n"
            << "  --reverse            Invert sort order\n"
            << "  --limit <N>          Limit output to first N records\n\n"
            << "Output Formatting Flags:\n"
            << "  --plain              Print bare titles, one per line\n"
            << "  --ids                Print bare IDs, one per line\n"
            << "  --csv                Output RFC 4180 CSV\n"
            << "  --json               Output structured JSON\n";
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
  if (command == "import") return executeImport(args, db);

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
    } else if (arg == "--reverse") {
      opts.reverseSort = true;
    } else if (arg == "--unrated") {
      opts.unratedOnly = true;
      opts.minScore = 0;
      opts.maxScore = 0;
    } else if (arg == "--type") {
      if (++i >= args.size()) { err = "missing argument for '--type'"; return false; }
      std::string t = args[i];
      std::transform(t.begin(), t.end(), t.begin(), ::tolower);
      if (t == "anime") opts.type = MediaType::Anime;
      else if (t == "manga") opts.type = MediaType::Manga;
      else { err = "invalid media type '" + args[i] + "'. Expected 'anime' or 'manga'."; return false; }
    } else if (arg == "--status") {
      if (++i >= args.size()) { err = "missing argument for '--status'"; return false; }
      opts.status = stringToStatus(args[i]);
    } else if (arg == "--rating") {
      if (++i >= args.size()) { err = "missing argument for '--rating'"; return false; }
      if (!CliDispatcher::parseRatingSpec(args[i], opts.minScore, opts.maxScore, opts.unratedOnly)) {
        err = "invalid rating specification '" + args[i] + "'. Expected e.g. 9, '>=9', '<=7', '6..8', or 'unrated'.";
        return false;
      }
    } else if (arg == "--year") {
      if (++i >= args.size()) { err = "missing argument for '--year'"; return false; }
      opts.year = args[i];
    } else if (arg == "--search") {
      if (++i >= args.size()) { err = "missing argument for '--search'"; return false; }
      opts.search = args[i];
    } else if (arg == "--limit") {
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

  // Tabular output (interactive TTY or piped)
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

    // Check if integer ID
    bool isInt = !q.empty() && std::all_of(q.begin(), q.end(), ::isdigit);
    if (isInt) {
      int id = std::stoi(q);
      match = db.getById(MediaType::Anime, id);
      if (!match.has_value()) {
        match = db.getById(MediaType::Manga, id);
      }
    }

    if (!match.has_value()) {
      // Search by title match
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

} // namespace catalist
