#ifndef CATALIST_DATABASE_HPP_
#define CATALIST_DATABASE_HPP_

#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "catalist/core/clock.hpp"
#include "catalist/core/result.hpp"
#include "catalist/model/activity.hpp"
#include "catalist/model/entry.hpp"
#include "catalist/model/syncBase.hpp"
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
  bool removeEntry(mediaType type, int id);
  void clear(mediaType type);
  static void sortEntries(std::vector<entry>& v, sortKey key, bool reverse);

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

  // Whether a catalogue file was found; fails, naming the file, when one
  // cannot be read, and then nothing of that file is loaded.
  [[nodiscard]] result<bool> loadAll();
  bool saveAll() const;
  // Whether a saveAll() failed since the last call. The CLI dispatcher and
  // the TUI shell each ask once, so the callers of saveAll() need not.
  [[nodiscard]] bool takeSaveFailure();
  [[nodiscard]] result<bool> loadCsv(mediaType type,
                                     const std::filesystem::path& path);
  bool saveCsv(mediaType type, const std::filesystem::path& path) const;

  void logActivity(const activityRecord& record);
  std::vector<activityRecord> getRecentActivity(mediaType type,
                                                size_t limit = 3) const;
  bool loadHistory();
  bool saveHistory() const;

  // The sync bases of `service` for `type`, by the service's id (see
  // syncBase). They live in sync.csv, which saveAll() writes last.
  [[nodiscard]] std::map<int, entry> syncBases(const std::string& service,
                                               mediaType type) const;
  void setSyncBase(const std::string& service, int remoteId,
                   const entry& values);
  void eraseSyncBase(const std::string& service, mediaType type, int remoteId);

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
  std::vector<syncBase> syncBases_;
  int nextLocalId_ = 10001;
  mutable bool saveFailed_ = false;

  std::vector<entry>& getList(mediaType type);
  const std::vector<entry>& getList(mediaType type) const;
  void loadSyncBases();
  bool saveSyncBases() const;
};

}  // namespace catalist

#endif  // CATALIST_DATABASE_HPP_
