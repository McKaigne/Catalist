#pragma once

#include <optional>
#include <string>
#include <vector>

#include "catalist/config/config.hpp"
#include "catalist/core/clock.hpp"
#include "catalist/platform/fileSystem.hpp"
#include "catalist/platform/httpClient.hpp"
#include "catalist/storage/database.hpp"
#include "catalist/net/remoteSearch.hpp"

namespace catalist {

enum class outputFormat { kTable, kPlain, kIds, kCsv, kJson };

struct cliFilterOptions {
  std::optional<mediaType> type;
  std::optional<watchStatus> status;
  int minScore = -1;
  int maxScore = -1;
  bool unratedOnly = false;
  std::string year;
  std::string search;
  std::string tag;
  sortKey sortBy = sortKey::kScore;
  bool reverseSort = false;
  int limit = -1;
  outputFormat format = outputFormat::kTable;
};

struct cliServices {
  const iClock& clock;
  iHttpClient& http;
  iFileSystem& fileSystem;
};

class cliDispatcher {
public:
  static int dispatch(int argc, char* argv[], database& db, appConfig& config,
                      const cliServices& services);

  static bool parseRatingSpec(const std::string& spec, int& minScore,
                              int& maxScore, bool& unratedOnly);
  static std::string formatJsonEntry(const entry& e);
  static std::string formatJsonEntries(const std::vector<entry>& entries);
  static void printUsage();

  static int executeLs(const std::vector<std::string>& args, database& db,
                       const appConfig& config);
  static int executeCount(const std::vector<std::string>& args, database& db);
  static int executeShow(const std::vector<std::string>& args, database& db);
  static int executeAdd(const std::vector<std::string>& args, database& db,
                        const iClock& clock);
  static int executeSet(const std::vector<std::string>& args, database& db,
                        const iClock& clock);
  static int executeRm(const std::vector<std::string>& args, database& db);
  static int executeRandom(const std::vector<std::string>& args, database& db);
  static int executeStats(const std::vector<std::string>& args,
                          const database& db);
  static int executeExport(const std::vector<std::string>& args,
                           const database& db);
  static int executeImport(const std::vector<std::string>& args, database& db,
                           const iFileSystem& fileSystem);
  static int executeSearch(const std::vector<std::string>& args, database& db,
                           iHttpClient& http);
  static int executeConfig(const std::vector<std::string>& args,
                           appConfig& config, iFileSystem& fileSystem,
                           const std::filesystem::path& configPath);
  static int executeCompletion(const std::vector<std::string>& args);
};

}  // namespace catalist
