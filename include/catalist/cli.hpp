#pragma once

#include "catalist/database.hpp"
#include "catalist/config.hpp"
#include <string>
#include <vector>
#include <optional>

namespace catalist {

enum class OutputFormat {
  Table,
  Plain,
  Ids,
  Csv,
  Json
};

struct CliFilterOptions {
  std::optional<MediaType> type;
  std::optional<WatchStatus> status;
  int minScore = -1;
  int maxScore = -1;
  bool unratedOnly = false;
  std::string year;
  std::string search;
  std::string tag;
  SortKey sortKey = SortKey::Score;
  bool reverseSort = false;
  int limit = -1;
  OutputFormat format = OutputFormat::Table;
};

class CliDispatcher {
public:
  static int dispatch(int argc, char* argv[], Database& db, AppConfig& config);

  static bool parseRatingSpec(const std::string& spec, int& minScore, int& maxScore, bool& unratedOnly);
  static std::string formatJsonEntry(const Entry& e);
  static std::string formatJsonEntries(const std::vector<Entry>& entries);
  static void printUsage();

  static int executeLs(const std::vector<std::string>& args, Database& db, const AppConfig& config);
  static int executeCount(const std::vector<std::string>& args, Database& db);
  static int executeShow(const std::vector<std::string>& args, Database& db);
  static int executeAdd(const std::vector<std::string>& args, Database& db);
  static int executeSet(const std::vector<std::string>& args, Database& db);
  static int executeRm(const std::vector<std::string>& args, Database& db);
  static int executeRandom(const std::vector<std::string>& args, Database& db);
  static int executeStats(const std::vector<std::string>& args, const Database& db);
  static int executeExport(const std::vector<std::string>& args, const Database& db);
  static int executeImport(const std::vector<std::string>& args, Database& db);
};

} // namespace catalist
