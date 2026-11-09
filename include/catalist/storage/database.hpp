#ifndef CATALIST_DATABASE_HPP_
#define CATALIST_DATABASE_HPP_

#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "catalist/core/clock.hpp"
#include "catalist/model/activity.hpp"
#include "catalist/model/deletion.hpp"
#include "catalist/model/entry.hpp"
#include "catalist/platform/fileSystem.hpp"

namespace catalist {

enum class sortKey { kScore, kStatus, kTitle, kProgress, kDate, kId };

class database {
 public:
  database(iFileSystem& fileSystem, const iClock& clock,
           std::filesystem::path configDir);

  int addEntry(entry mediaEntry);
  const std::vector<entry>& getEntries(mediaType type) const;
  std::optional<entry> getById(mediaType type, int id) const;
  bool updateEntry(const entry& mediaEntry);
  // Entries that had been synced are remembered in pendingDeletions() so
  // the next sync can delete them remotely too.
  bool removeEntry(mediaType type, int id);
  [[nodiscard]] std::vector<deletedEntry> pendingDeletions() const;
  // Drops `service` from the matching deletion; forgets it once none remain.
  void settleDeletion(const deletedEntry& deleted, const std::string& service);
  bool changeMediaType(mediaType currentType, int id);
  void clear(mediaType type);
  static void sortEntries(std::vector<entry>& v, sortKey key, bool reverse);

  std::vector<entry> filterEntries(mediaType type,
                                   std::optional<watchStatus> status,
                                   std::optional<int> exactScore,
                                   const std::string& query, sortKey sortBy,
                                   bool reverse) const;

  size_t countByStatus(mediaType type, watchStatus status) const;
  size_t countByScore(mediaType type, int score) const;
  double averageScore(mediaType type) const;
  int totalProgress(mediaType type) const;
  int totalVolumes(mediaType type) const;
  int totalRewatched(mediaType type) const;
  double daysSpent(mediaType type) const;
  double completedPercentage(mediaType type) const;

  // CATALIST_DATA_DIR, then $XDG_CONFIG_HOME/catalist, then
  // ~/.config/catalist.
  static std::filesystem::path getDefaultConfigDir();
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
  [[nodiscard]] std::vector<activityRecord> getHistory() const;
  bool loadHistory();
  bool saveHistory() const;

  void createBackupSnapshot();
  void autoHealIds();

 private:
  void trackLocalId(int id);

  iFileSystem& fileSystem_;
  const iClock& clock_;
  std::filesystem::path configDir_;
  mutable std::recursive_mutex mutex_;
  std::vector<entry> animeEntries_;
  std::vector<entry> mangaEntries_;
  std::vector<activityRecord> historyRecords_;
  std::vector<deletedEntry> deletions_;
  int nextLocalId_ = 10001;

  std::vector<entry>& getList(mediaType type);
  const std::vector<entry>& getList(mediaType type) const;
  void forgetDeletionsOf(const entry& restored);
  void loadDeletions();
  bool saveDeletions() const;
};

}  // namespace catalist

#endif  // CATALIST_DATABASE_HPP_
