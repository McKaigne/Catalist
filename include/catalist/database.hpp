#pragma once

#include "catalist/entry.hpp"
#include <vector>
#include <string>
#include <optional>
#include <filesystem>

namespace catalist {

enum class SortKey {
  Score,
  Status,
  Title,
  Progress,
  Date,
  Id
};

struct ActivityRecord {
  std::string timestamp;
  MediaType type = MediaType::Anime;
  std::string title;
  WatchStatus status = WatchStatus::Watching;
  int progress = 0;
  int totalUnits = 0;
  int score = 0;
};

class Database {
public:
  Database();
  explicit Database(std::filesystem::path configDir);

  int addEntry(Entry entry);
  const std::vector<Entry>& getEntries(MediaType type) const;
  std::optional<Entry> getById(MediaType type, int id) const;
  bool updateEntry(const Entry& entry);
  bool removeEntry(MediaType type, int id);
  bool changeMediaType(MediaType currentType, int id);
  void clear(MediaType type);
  static void sortEntries(std::vector<Entry>& v, SortKey key, bool reverse);

  std::vector<Entry> filterEntries(MediaType type,
                                   std::optional<WatchStatus> status,
                                   int scoreFilter,
                                   const std::string& query,
                                   SortKey sortKey,
                                   bool reverse) const;

  size_t countByStatus(MediaType type, WatchStatus status) const;
  size_t countByScore(MediaType type, int score) const;
  double averageScore(MediaType type) const;
  int totalProgress(MediaType type) const;
  int totalVolumes(MediaType type) const;
  int totalRewatched(MediaType type) const;
  double daysSpent(MediaType type) const;
  double completedPercentage(MediaType type) const;

  static std::filesystem::path getDefaultConfigDir();
  static std::filesystem::path getConfigDir();
  std::filesystem::path getConfigPath() const;
  std::filesystem::path getCsvPath(MediaType type) const;
  std::filesystem::path getHistoryPath() const;
  std::filesystem::path getBackupDir() const;

  bool loadAll();
  bool saveAll() const;
  bool loadCsv(MediaType type, const std::filesystem::path& path);
  bool saveCsv(MediaType type, const std::filesystem::path& path) const;

  void logActivity(const ActivityRecord& record);
  std::vector<ActivityRecord> getRecentActivity(MediaType type, size_t limit = 3) const;
  bool loadHistory();
  bool saveHistory() const;

  void createBackupSnapshot();
  void autoHealIds();
  void seedDefaults();

private:
  std::filesystem::path configDir_;
  std::vector<Entry> animeEntries_;
  std::vector<Entry> mangaEntries_;
  std::vector<ActivityRecord> historyRecords_;
  int nextLocalId_ = 10001;

  std::vector<Entry>& getList(MediaType type);
  const std::vector<Entry>& getList(MediaType type) const;
};

} // namespace catalist
