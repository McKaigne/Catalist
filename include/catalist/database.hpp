#ifndef CATALIST_DATABASE_HPP_
#define CATALIST_DATABASE_HPP_

#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "catalist/entry.hpp"

namespace catalist {

enum class sortKey { Score, Status, Title, Progress, Date, Id };

struct activityRecord {
  std::string timestamp;
  mediaType type = mediaType::Anime;
  std::string title;
  watchStatus status = watchStatus::Watching;
  int progress = 0;
  int totalUnits = 0;
  int score = 0;
};

class database {
 public:
  database();
  explicit database(std::filesystem::path configDir);

  int addEntry(entry mediaEntry);
  const std::vector<entry>& getEntries(mediaType type) const;
  std::optional<entry> getById(mediaType type, int id) const;
  bool updateEntry(const entry& mediaEntry);
  bool removeEntry(mediaType type, int id);
  bool changeMediaType(mediaType currentType, int id);
  void clear(mediaType type);
  static void sortEntries(std::vector<entry>& v, sortKey key, bool reverse);

  std::vector<entry> filterEntries(mediaType type,
                                   std::optional<watchStatus> status,
                                   int scoreFilter, const std::string& query,
                                   sortKey sortBy, bool reverse) const;

  size_t countByStatus(mediaType type, watchStatus status) const;
  size_t countByScore(mediaType type, int score) const;
  double averageScore(mediaType type) const;
  int totalProgress(mediaType type) const;
  int totalVolumes(mediaType type) const;
  int totalRewatched(mediaType type) const;
  double daysSpent(mediaType type) const;
  double completedPercentage(mediaType type) const;

  static std::filesystem::path getDefaultConfigDir();
  static std::filesystem::path getConfigDir();
  std::filesystem::path getConfigPath() const;
  std::filesystem::path getCsvPath(mediaType type) const;
  std::filesystem::path getHistoryPath() const;
  std::filesystem::path getBackupDir() const;

  bool loadAll();
  bool saveAll() const;
  bool loadCsv(mediaType type, const std::filesystem::path& path);
  bool saveCsv(mediaType type, const std::filesystem::path& path) const;

  void logActivity(const activityRecord& record);
  std::vector<activityRecord> getRecentActivity(mediaType type,
                                                size_t limit = 3) const;
  bool loadHistory();
  bool saveHistory() const;

  void createBackupSnapshot();
  void autoHealIds();
  void seedDefaults();

 private:
  std::filesystem::path configDir_;
  mutable std::recursive_mutex mutex_;
  std::vector<entry> animeEntries_;
  std::vector<entry> mangaEntries_;
  std::vector<activityRecord> historyRecords_;
  int nextLocalId_ = 10001;

  std::vector<entry>& getList(mediaType type);
  const std::vector<entry>& getList(mediaType type) const;
};

}  // namespace catalist

#endif  // CATALIST_DATABASE_HPP_
