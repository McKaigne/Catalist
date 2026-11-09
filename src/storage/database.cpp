#include "catalist/storage/database.hpp"
#include "catalist/core/logger.hpp"

#include <algorithm>
#include <cstdlib>
#include <mutex>

#include "catalist/storage/backup.hpp"
#include "catalist/storage/csvCodec.hpp"

namespace catalist {

namespace {

// Enough for years of daily logging while keeping history.csv small.
constexpr size_t kHistoryLimit = 5000;

}  // namespace

std::filesystem::path database::getDefaultConfigDir() {
  const char* custom = std::getenv("CATALIST_DATA_DIR");
  if (custom && *custom)
    return std::filesystem::path(custom);

  const char* xdg = std::getenv("XDG_CONFIG_HOME");
  if (xdg && *xdg)
    return std::filesystem::path(xdg) / "catalist";

  const char* home = std::getenv("HOME");
  if (home && *home)
    return std::filesystem::path(home) / ".config" / "catalist";

  const char* appdata = std::getenv("APPDATA");
  if (appdata && *appdata)
    return std::filesystem::path(appdata) / "catalist";

  return std::filesystem::path("./config");
}

database::database(iFileSystem& fileSystem, const iClock& clock,
                   std::filesystem::path configDir)
    : fileSystem_(fileSystem), clock_(clock), configDir_(std::move(configDir)) {}

std::filesystem::path database::getConfigPath() const {
  return configDir_;
}

std::vector<entry>& database::getList(mediaType type) {
  return (type == mediaType::kAnime) ? animeEntries_ : mangaEntries_;
}

const std::vector<entry>& database::getList(mediaType type) const {
  return (type == mediaType::kAnime) ? animeEntries_ : mangaEntries_;
}

void database::clear(mediaType type) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  getList(type).clear();
}

void database::sortEntries(std::vector<entry>& v, sortKey key, bool reverse) {
  auto less = [key](const entry& a, const entry& b) {
    switch (key) {
      case sortKey::kScore:
        return a.score != b.score ? a.score > b.score : a.title < b.title;
      case sortKey::kProgress:
        return a.progress != b.progress ? a.progress > b.progress
                                        : a.title < b.title;
      case sortKey::kDate:
        return a.startDate != b.startDate ? a.startDate > b.startDate
                                          : a.title < b.title;
      case sortKey::kStatus:
        return a.status != b.status
                   ? static_cast<int>(a.status) < static_cast<int>(b.status)
                   : a.title < b.title;
      case sortKey::kTitle:
        return a.title < b.title;
      case sortKey::kId:
        return a.id < b.id;
    }
    return false;
  };
  std::sort(v.begin(), v.end(), [&](const entry& a, const entry& b) {
    return reverse ? less(b, a) : less(a, b);
  });
}

int database::addEntry(entry mediaEntry) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  LOG_INFO("Adding/updating entry ID " + std::to_string(mediaEntry.id) + ": " + mediaEntry.title);
  if (mediaEntry.malId > 0 && mediaEntry.id <= 0) {
    mediaEntry.id = mediaEntry.malId;
  } else if (mediaEntry.id <= 0) {
    mediaEntry.id = nextLocalId_++;
  }

  if (mediaEntry.id >= nextLocalId_) {
    nextLocalId_ = mediaEntry.id + 1;
  }

  auto& list = getList(mediaEntry.type);
  for (auto& existing : list) {
    bool idMatch = (existing.id == mediaEntry.id);
    bool malMatch = (mediaEntry.malId > 0 && existing.malId == mediaEntry.malId);
    bool remoteMatch = false;
    std::string incMal = mediaEntry.getRemoteId("mal");
    if (!incMal.empty() && existing.getRemoteId("mal") == incMal) {
      remoteMatch = true;
    }

    if (idMatch || malMatch || remoteMatch) {
      // Tier 2 (Upstream metadata)
      if (mediaEntry.totalUnits > 0)
        existing.totalUnits = mediaEntry.totalUnits;
      if (mediaEntry.totalVolumes > 0)
        existing.totalVolumes = mediaEntry.totalVolumes;
      for (const auto& [prov, rId] : mediaEntry.remoteIds) {
        existing.setRemoteId(prov, rId);
      }
      if (mediaEntry.malId > 0 && existing.malId <= 0)
        existing.malId = mediaEntry.malId;
      if (mediaEntry.anilistId > 0 && existing.anilistId <= 0)
        existing.anilistId = mediaEntry.anilistId;

      // Tier 1 (User Authority): Only backfill untouched local records
      if (existing.notes.empty() && !mediaEntry.notes.empty())
        existing.notes = mediaEntry.notes;
      if (existing.score == 0 && mediaEntry.score > 0)
        existing.score = mediaEntry.score;
      if (existing.progress == 0 && mediaEntry.progress > 0)
        existing.progress = mediaEntry.progress;
      if (!existing.startDate && mediaEntry.startDate)
        existing.startDate = mediaEntry.startDate;
      if (!existing.finishDate && mediaEntry.finishDate)
        existing.finishDate = mediaEntry.finishDate;

      return existing.id;
    }
  }

  list.push_back(mediaEntry);
  return mediaEntry.id;
}

const std::vector<entry>& database::getEntries(mediaType type) const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  return getList(type);
}

std::optional<entry> database::getById(mediaType type, int id) const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  const auto& list = getList(type);
  // Prioritize exact ID match
  for (const auto& item : list) {
    if (item.id == id)
      return item;
  }
  // Fall back to matching upstream provider MAL ID
  for (const auto& item : list) {
    if (item.malId > 0 && item.malId == id)
      return item;
  }
  return std::nullopt;
}

bool database::updateEntry(const entry& mediaEntry) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  auto& list = getList(mediaEntry.type);
  for (auto& item : list) {
    if (item.id == mediaEntry.id) {
      item = mediaEntry;
      return true;
    }
  }
  return false;
}

bool database::removeEntry(mediaType type, int id) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  LOG_INFO("Removing entry ID " + std::to_string(id));
  auto& list = getList(type);
  auto it = std::remove_if(list.begin(), list.end(),
                           [id](const entry& e) { return e.id == id; });
  if (it != list.end()) {
    list.erase(it, list.end());
    return true;
  }
  return false;
}

bool database::changeMediaType(mediaType currentType, int id) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  auto& srcList = getList(currentType);
  mediaType targetType =
      (currentType == mediaType::kAnime) ? mediaType::kManga : mediaType::kAnime;
  auto& dstList = getList(targetType);

  auto it = std::find_if(srcList.begin(), srcList.end(),
                         [id](const entry& e) { return e.id == id; });
  if (it == srcList.end())
    return false;

  entry moved = *it;
  moved.type = targetType;
  srcList.erase(it);
  dstList.push_back(moved);
  saveAll();
  return true;
}

std::vector<entry> database::filterEntries(
    mediaType type, std::optional<watchStatus> status,
    std::optional<int> exactScore, const std::string& query, sortKey sortBy,
    bool reverse) const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  std::vector<entry> results;
  const auto& list = getList(type);

  std::string queryLower = query;
  std::transform(queryLower.begin(), queryLower.end(), queryLower.begin(),
                 ::tolower);

  for (const auto& e : list) {
    if (status.has_value() && e.status != status.value())
      continue;
    if (exactScore && e.score != *exactScore)
      continue;

    if (!queryLower.empty()) {
      std::string titleLower = e.title;
      std::transform(titleLower.begin(), titleLower.end(), titleLower.begin(),
                     ::tolower);
      if (titleLower.find(queryLower) == std::string::npos)
        continue;
    }

    results.push_back(e);
  }

  sortEntries(results, sortBy, reverse);
  return results;
}

size_t database::countByStatus(mediaType type, watchStatus status) const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  size_t count = 0;
  for (const auto& e : getList(type)) {
    if (e.status == status)
      count++;
  }
  return count;
}

size_t database::countByScore(mediaType type, int score) const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  size_t count = 0;
  for (const auto& e : getList(type)) {
    if (e.score == score)
      count++;
  }
  return count;
}

double database::averageScore(mediaType type) const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  int sum = 0;
  int rated = 0;
  for (const auto& e : getList(type)) {
    if (e.score > 0) {
      sum += e.score;
      rated++;
    }
  }
  return (rated == 0) ? 0.0 : static_cast<double>(sum) / rated;
}

int database::totalProgress(mediaType type) const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  int total = 0;
  for (const auto& e : getList(type)) {
    total += e.progress;
  }
  return total;
}

int database::totalVolumes(mediaType type) const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  int total = 0;
  for (const auto& e : getList(type)) {
    total += e.volumes;
  }
  return total;
}

int database::totalRewatched(mediaType type) const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  int total = 0;
  for (const auto& e : getList(type)) {
    total += e.rewatched;
  }
  return total;
}

double database::daysSpent(mediaType type) const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  if (type == mediaType::kAnime) {
    return (totalProgress(mediaType::kAnime) * 24.0) / 1440.0;
  } else {
    return (totalProgress(mediaType::kManga) * 5.0) / 1440.0;
  }
}

double database::completedPercentage(mediaType type) const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  const auto& list = getList(type);
  if (list.empty())
    return 0.0;
  return (static_cast<double>(countByStatus(type, watchStatus::kCompleted)) /
          list.size()) *
         100.0;
}

std::filesystem::path database::getCsvPath(mediaType type) const {
  return configDir_ / csvCodec::catalogFileName(type);
}

std::filesystem::path database::getHistoryPath() const {
  return configDir_ / csvCodec::historyFileName();
}

std::filesystem::path database::getBackupDir() const {
  return backup::backupDirectory(configDir_);
}

void database::createBackupSnapshot() {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  LOG_INFO("Creating pre-mutation snapshot backup...");
  backup::createSnapshot(fileSystem_, clock_, configDir_);
}

void database::trackLocalId(int id) {
  if (id >= nextLocalId_)
    nextLocalId_ = id + 1;
}

void database::autoHealIds() {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  for (const auto& e : animeEntries_)
    trackLocalId(e.id);
  for (const auto& e : mangaEntries_)
    trackLocalId(e.id);
}

bool database::loadCsv(mediaType type, const std::filesystem::path& path) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  auto content = fileSystem_.readFile(path);
  if (!content)
    return false;
  auto& list = getList(type);
  list = csvCodec::decodeEntries(*content, type);
  for (const auto& e : list)
    trackLocalId(e.id);
  return true;
}

bool database::saveCsv(mediaType type,
                       const std::filesystem::path& path) const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  return fileSystem_.writeFileAtomic(path,
                                     csvCodec::encodeEntries(getList(type)));
}

void database::logActivity(const activityRecord& record) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  historyRecords_.insert(historyRecords_.begin(), record);
  if (historyRecords_.size() > kHistoryLimit)
    historyRecords_.resize(kHistoryLimit);
  saveHistory();
}

std::vector<activityRecord> database::getRecentActivity(mediaType type,
                                                        size_t limit) const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  std::vector<activityRecord> res;
  for (const auto& h : historyRecords_) {
    if (h.type == type) {
      res.push_back(h);
      if (res.size() >= limit)
        break;
    }
  }
  return res;
}

std::vector<activityRecord> database::getHistory() const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  return historyRecords_;
}

bool database::loadHistory() {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  auto content = fileSystem_.readFile(getHistoryPath());
  if (!content)
    return false;
  historyRecords_ = csvCodec::decodeHistory(*content);
  return true;
}

bool database::saveHistory() const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  return fileSystem_.writeFileAtomic(getHistoryPath(),
                                     csvCodec::encodeHistory(historyRecords_));
}

bool database::loadAll() {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  bool a = loadCsv(mediaType::kAnime, getCsvPath(mediaType::kAnime));
  bool m = loadCsv(mediaType::kManga, getCsvPath(mediaType::kManga));
  loadHistory();
  autoHealIds();
  return a || m;
}

bool database::saveAll() const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  fileSystem_.createDirectories(configDir_);
  bool a = saveCsv(mediaType::kAnime, getCsvPath(mediaType::kAnime));
  bool m = saveCsv(mediaType::kManga, getCsvPath(mediaType::kManga));
  saveHistory();
  return a && m;
}

}  // namespace catalist
