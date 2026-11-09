#include "catalist/database.hpp"
#include "catalist/core/logger.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>

namespace catalist {

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

std::filesystem::path database::getConfigDir() {
  return getDefaultConfigDir();
}

database::database() : configDir_(getDefaultConfigDir()) {}
database::database(std::filesystem::path configDir)
    : configDir_(std::move(configDir)) {}

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
      if (existing.startDate.empty() && !mediaEntry.startDate.empty())
        existing.startDate = mediaEntry.startDate;
      if (existing.finishDate.empty() && !mediaEntry.finishDate.empty())
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
    mediaType type, std::optional<watchStatus> status, int scoreFilter,
    const std::string& query, sortKey sortBy, bool reverse) const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  std::vector<entry> results;
  const auto& list = getList(type);

  std::string queryLower = query;
  std::transform(queryLower.begin(), queryLower.end(), queryLower.begin(),
                 ::tolower);

  for (const auto& e : list) {
    if (status.has_value() && e.status != status.value())
      continue;
    if (scoreFilter >= 0 && scoreFilter <= 10 && e.score != scoreFilter)
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
  return configDir_ / (type == mediaType::kAnime ? "anime.csv" : "manga.csv");
}

std::filesystem::path database::getHistoryPath() const {
  return configDir_ / "history.csv";
}

std::filesystem::path database::getBackupDir() const {
  return configDir_ / "backups";
}

void database::createBackupSnapshot() {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  LOG_INFO("Creating pre-mutation snapshot backup...");
  auto bdir = getBackupDir();
  std::filesystem::create_directories(bdir);

  auto now = std::chrono::system_clock::now();
  auto inTimeT = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
  ss << std::put_time(std::localtime(&inTimeT), "%Y%m%d_%H%M%S");
  std::string timestamp = ss.str();

  if (std::filesystem::exists(getCsvPath(mediaType::kAnime))) {
    std::filesystem::copy_file(
        getCsvPath(mediaType::kAnime), bdir / ("anime_" + timestamp + ".csv"),
        std::filesystem::copy_options::overwrite_existing);
  }
  if (std::filesystem::exists(getCsvPath(mediaType::kManga))) {
    std::filesystem::copy_file(
        getCsvPath(mediaType::kManga), bdir / ("manga_" + timestamp + ".csv"),
        std::filesystem::copy_options::overwrite_existing);
  }
}

static std::vector<std::string> parseCsvRecord(const std::string& record) {
  std::vector<std::string> fields;
  std::string field;
  bool inQuotes = false;

  for (size_t i = 0; i < record.size(); ++i) {
    char c = record[i];
    if (inQuotes) {
      if (c == '"') {
        if (i + 1 < record.size() && record[i + 1] == '"') {
          field += '"';
          ++i;
        } else {
          inQuotes = false;
        }
      } else {
        field += c;
      }
    } else {
      if (c == '"') {
        inQuotes = true;
      } else if (c == ',') {
        fields.push_back(field);
        field.clear();
      } else {
        field += c;
      }
    }
  }
  fields.push_back(field);
  return fields;
}

void database::autoHealIds() {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  for (const auto& e : animeEntries_) {
    if (e.id >= nextLocalId_)
      nextLocalId_ = e.id + 1;
  }
  for (const auto& e : mangaEntries_) {
    if (e.id >= nextLocalId_)
      nextLocalId_ = e.id + 1;
  }
}

bool database::loadCsv(mediaType type, const std::filesystem::path& path) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  std::ifstream file(path);
  if (!file.is_open())
    return false;

  auto& list = getList(type);
  list.clear();

  std::string line;
  std::string recordBuffer;
  bool insideQuotes = false;

  while (std::getline(file, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (recordBuffer.empty() && (line.empty() || line[0] == '#'))
      continue;

    for (size_t i = 0; i < line.size(); ++i) {
      if (line[i] == '"') {
        if (insideQuotes && i + 1 < line.size() && line[i + 1] == '"') {
          ++i;
        } else {
          insideQuotes = !insideQuotes;
        }
      }
    }

    recordBuffer += (recordBuffer.empty() ? "" : "\n") + line;

    if (!insideQuotes) {
      auto fields = parseCsvRecord(recordBuffer);
      recordBuffer.clear();

      if (fields.size() < 7)
        continue;

      entry e;
      e.type = type;

      try {
        e.id = std::stoi(fields[0]);
      } catch (...) {
        continue;
      }
      try {
        e.malId = std::stoi(fields[1]);
      } catch (...) {
        e.malId = 0;
      }
      if (fields.size() > 2 && !fields[2].empty()) {
        try {
          e.anilistId = std::stoi(fields[2]);
        } catch (...) {
          e.anilistId = 0;
        }
      }

      size_t offset = 2;
      if (fields.size() >= 4 &&
          (fields[3] == "anime" || fields[3] == "manga")) {
        offset = 4;
      } else if (fields.size() >= 16) {
        offset = 3;
      }

      e.title = fields[offset];
      e.status = stringToStatus(fields[offset + 1]);
      try {
        e.score = std::stoi(fields[offset + 2]);
      } catch (...) {
        e.score = 0;
      }
      try {
        e.progress = std::stoi(fields[offset + 3]);
      } catch (...) {
        e.progress = 0;
      }
      try {
        e.totalUnits = std::stoi(fields[offset + 4]);
      } catch (...) {
        e.totalUnits = 0;
      }

      if (fields.size() > offset + 5) {
        try {
          e.volumes = std::stoi(fields[offset + 5]);
        } catch (...) {
        }
      }
      if (fields.size() > offset + 6) {
        try {
          e.totalVolumes = std::stoi(fields[offset + 6]);
        } catch (...) {
        }
      }
      if (fields.size() > offset + 7) {
        try {
          e.rewatched = std::stoi(fields[offset + 7]);
        } catch (...) {
        }
      }
      if (fields.size() > offset + 8)
        e.startDate = fields[offset + 8];
      if (fields.size() > offset + 9)
        e.finishDate = fields[offset + 9];
      if (fields.size() > offset + 10)
        e.notes = fields[offset + 10];
      if (fields.size() > offset + 11)
        e.updatedAt = fields[offset + 11];
      if (fields.size() > offset + 12) {
        e.dirty = (fields[offset + 12] == "1" || fields[offset + 12] == "true");
      }
      if (fields.size() > offset + 13 && !fields[offset + 13].empty()) {
        e.remoteIds = deserializeRemoteIds(fields[offset + 13]);
      }
      if (e.malId > 0 && e.remoteIds.find("mal") == e.remoteIds.end()) {
        e.remoteIds["mal"] = std::to_string(e.malId);
      }
      if (e.anilistId > 0 && e.remoteIds.find("anilist") == e.remoteIds.end()) {
        e.remoteIds["anilist"] = std::to_string(e.anilistId);
      }

      if (e.id >= nextLocalId_) {
        nextLocalId_ = e.id + 1;
      }

      list.push_back(e);
    }
  }

  return true;
}

bool database::saveCsv(mediaType type,
                       const std::filesystem::path& path) const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  std::filesystem::create_directories(path.parent_path());
  auto tmpPath = path.string() + ".tmp";
  std::ofstream file(tmpPath);
  if (!file.is_open())
    return false;

  file
      << "#id,mal_id,anilist_id,type,title,status,score,progress,total_units,volumes,total_volumes,rewatched,start_date,finish_date,notes,updated_at,dirty,external_ids\n";
  for (const auto& e : getList(type)) {
    file << e.id << "," << e.malId << "," << e.anilistId << ","
         << (e.type == mediaType::kAnime ? "anime" : "manga") << ","
         << escapeCsv(e.title) << ","
         << escapeCsv(statusToString(e.status, e.type)) << "," << e.score << ","
         << e.progress << "," << e.totalUnits << "," << e.volumes << ","
         << e.totalVolumes << "," << e.rewatched << ","
         << escapeCsv(e.startDate) << "," << escapeCsv(e.finishDate) << ","
         << escapeCsv(e.notes) << "," << escapeCsv(e.updatedAt) << ","
         << (e.dirty ? "1" : "0") << ","
         << escapeCsv(serializeRemoteIds(e.remoteIds)) << "\n";
  }
  file.close();
  std::error_code ec;
  std::filesystem::rename(tmpPath, path, ec);
  return !ec;
}

void database::logActivity(const activityRecord& record) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  historyRecords_.insert(historyRecords_.begin(), record);
  if (historyRecords_.size() > 100) {
    historyRecords_.resize(100);
  }
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

bool database::loadHistory() {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  auto hPath = getHistoryPath();
  std::ifstream file(hPath);
  if (!file.is_open())
    return false;

  historyRecords_.clear();
  std::string line;
  while (std::getline(file, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (line.empty() || line[0] == '#')
      continue;
    auto fields = parseCsvRecord(line);
    if (fields.size() < 7)
      continue;

    activityRecord r;
    r.timestamp = fields[0];
    r.type = (fields[1] == "anime" ? mediaType::kAnime : mediaType::kManga);
    r.title = fields[2];
    r.status = stringToStatus(fields[3]);
    try {
      r.progress = std::stoi(fields[4]);
    } catch (...) {
      r.progress = 0;
    }
    try {
      r.totalUnits = std::stoi(fields[5]);
    } catch (...) {
      r.totalUnits = 0;
    }
    try {
      r.score = std::stoi(fields[6]);
    } catch (...) {
      r.score = 0;
    }
    historyRecords_.push_back(r);
  }
  return true;
}

bool database::saveHistory() const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  std::filesystem::create_directories(configDir_);
  auto hPath = getHistoryPath();
  auto tmpPath = hPath.string() + ".tmp";
  std::ofstream file(tmpPath);
  if (!file.is_open())
    return false;

  file << "#timestamp,type,title,status,progress,total_units,score\n";
  for (const auto& h : historyRecords_) {
    file << escapeCsv(h.timestamp) << ","
         << (h.type == mediaType::kAnime ? "anime" : "manga") << ","
         << escapeCsv(h.title) << ","
         << escapeCsv(statusToString(h.status, h.type)) << "," << h.progress
         << "," << h.totalUnits << "," << h.score << "\n";
  }
  file.close();
  std::error_code ec;
  std::filesystem::rename(tmpPath, hPath, ec);
  return !ec;
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
  std::filesystem::create_directories(configDir_);
  bool a = saveCsv(mediaType::kAnime, getCsvPath(mediaType::kAnime));
  bool m = saveCsv(mediaType::kManga, getCsvPath(mediaType::kManga));
  saveHistory();
  return a && m;
}

void database::seedDefaults() {}

}  // namespace catalist
