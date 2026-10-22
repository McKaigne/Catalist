#include "catalist/database.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <chrono>
#include <iomanip>

namespace catalist {

std::filesystem::path Database::getDefaultConfigDir() {
  const char* custom = std::getenv("CATALIST_DATA_DIR");
  if (custom && *custom) return std::filesystem::path(custom);

  const char* xdg = std::getenv("XDG_CONFIG_HOME");
  if (xdg && *xdg) return std::filesystem::path(xdg) / "catalist";

  const char* home = std::getenv("HOME");
  if (home && *home) return std::filesystem::path(home) / ".config" / "catalist";

  const char* appdata = std::getenv("APPDATA");
  if (appdata && *appdata) return std::filesystem::path(appdata) / "catalist";

  return std::filesystem::path("./config");
}

std::filesystem::path Database::getConfigDir() {
  return getDefaultConfigDir();
}

Database::Database() : configDir_(getDefaultConfigDir()) {}
Database::Database(std::filesystem::path configDir) : configDir_(std::move(configDir)) {}

std::filesystem::path Database::getConfigPath() const {
  return configDir_;
}

std::vector<Entry>& Database::getList(MediaType type) {
  return (type == MediaType::Anime) ? animeEntries_ : mangaEntries_;
}

const std::vector<Entry>& Database::getList(MediaType type) const {
  return (type == MediaType::Anime) ? animeEntries_ : mangaEntries_;
}

void Database::clear(MediaType type) {
  getList(type).clear();
}

int Database::addEntry(Entry entry) {
  if (entry.malId > 0) {
    entry.id = entry.malId;
  } else if (entry.id <= 0) {
    entry.id = nextLocalId_++;
  }

  auto& list = getList(entry.type);
  for (auto& existing : list) {
    if (existing.id == entry.id) {
      existing = entry;
      return entry.id;
    }
  }

  list.push_back(entry);
  return entry.id;
}

const std::vector<Entry>& Database::getEntries(MediaType type) const {
  return getList(type);
}

std::optional<Entry> Database::getById(MediaType type, int id) const {
  for (const auto& item : getList(type)) {
    if (item.id == id) return item;
  }
  return std::nullopt;
}

bool Database::updateEntry(const Entry& entry) {
  auto& list = getList(entry.type);
  for (auto& item : list) {
    if (item.id == entry.id) {
      item = entry;
      return true;
    }
  }
  return false;
}

bool Database::removeEntry(MediaType type, int id) {
  auto& list = getList(type);
  auto it = std::remove_if(list.begin(), list.end(), [id](const Entry& e) { return e.id == id; });
  if (it != list.end()) {
    list.erase(it, list.end());
    return true;
  }
  return false;
}

std::vector<Entry> Database::filterEntries(MediaType type,
                                           std::optional<WatchStatus> status,
                                           int scoreFilter,
                                           const std::string& query,
                                           SortKey sortKey,
                                           bool reverse) const {
  std::vector<Entry> results;
  const auto& list = getList(type);

  std::string queryLower = query;
  std::transform(queryLower.begin(), queryLower.end(), queryLower.begin(), ::tolower);

  for (const auto& e : list) {
    if (status.has_value() && e.status != status.value()) continue;
    if (scoreFilter >= 0 && scoreFilter <= 10 && e.score != scoreFilter) continue;

    if (!queryLower.empty()) {
      std::string titleLower = e.title;
      std::transform(titleLower.begin(), titleLower.end(), titleLower.begin(), ::tolower);
      if (titleLower.find(queryLower) == std::string::npos) continue;
    }

    results.push_back(e);
  }

  std::sort(results.begin(), results.end(), [sortKey, reverse](const Entry& a, const Entry& b) {
    bool res = false;
    switch (sortKey) {
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
    return reverse ? !res : res;
  });

  return results;
}

size_t Database::countByStatus(MediaType type, WatchStatus status) const {
  size_t count = 0;
  for (const auto& e : getList(type)) {
    if (e.status == status) count++;
  }
  return count;
}

size_t Database::countByScore(MediaType type, int score) const {
  size_t count = 0;
  for (const auto& e : getList(type)) {
    if (e.score == score) count++;
  }
  return count;
}

double Database::averageScore(MediaType type) const {
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

int Database::totalProgress(MediaType type) const {
  int total = 0;
  for (const auto& e : getList(type)) {
    total += e.progress;
  }
  return total;
}

int Database::totalVolumes(MediaType type) const {
  int total = 0;
  for (const auto& e : getList(type)) {
    total += e.volumes;
  }
  return total;
}

int Database::totalRewatched(MediaType type) const {
  int total = 0;
  for (const auto& e : getList(type)) {
    total += e.rewatched;
  }
  return total;
}

double Database::daysSpent(MediaType type) const {
  if (type == MediaType::Anime) {
    return (totalProgress(MediaType::Anime) * 24.0) / 1440.0;
  } else {
    return (totalProgress(MediaType::Manga) * 5.0) / 1440.0;
  }
}

double Database::completedPercentage(MediaType type) const {
  const auto& list = getList(type);
  if (list.empty()) return 0.0;
  return (static_cast<double>(countByStatus(type, WatchStatus::Completed)) / list.size()) * 100.0;
}

std::filesystem::path Database::getCsvPath(MediaType type) const {
  return configDir_ / (type == MediaType::Anime ? "anime.csv" : "manga.csv");
}

std::filesystem::path Database::getHistoryPath() const {
  return configDir_ / "history.csv";
}

std::filesystem::path Database::getBackupDir() const {
  return configDir_ / "backups";
}

void Database::createBackupSnapshot() {
  auto bdir = getBackupDir();
  std::filesystem::create_directories(bdir);

  auto now = std::chrono::system_clock::now();
  auto inTimeT = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
  ss << std::put_time(std::localtime(&inTimeT), "%Y%m%d_%H%M%S");
  std::string timestamp = ss.str();

  if (std::filesystem::exists(getCsvPath(MediaType::Anime))) {
    std::filesystem::copy_file(getCsvPath(MediaType::Anime),
                               bdir / ("anime_" + timestamp + ".csv"),
                               std::filesystem::copy_options::overwrite_existing);
  }
  if (std::filesystem::exists(getCsvPath(MediaType::Manga))) {
    std::filesystem::copy_file(getCsvPath(MediaType::Manga),
                               bdir / ("manga_" + timestamp + ".csv"),
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

void Database::autoHealIds() {
  bool healed = false;
  auto healList = [&](std::vector<Entry>& list) {
    for (auto& e : list) {
      if (e.malId > 0 && e.id != e.malId) {
        e.id = e.malId;
        healed = true;
      }
    }
  };
  healList(animeEntries_);
  healList(mangaEntries_);
  if (healed) {
    saveAll();
  }
}

bool Database::loadCsv(MediaType type, const std::filesystem::path& path) {
  std::ifstream file(path);
  if (!file.is_open()) return false;

  auto& list = getList(type);
  list.clear();

  std::string line;
  std::string recordBuffer;
  bool insideQuotes = false;
  bool needsHeal = false;

  while (std::getline(file, line)) {
    if (recordBuffer.empty() && (line.empty() || line[0] == '#')) continue;

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

      if (fields.size() < 7) continue;

      Entry e;
      e.type = type;

      try { e.id = std::stoi(fields[0]); } catch (...) { continue; }
      try { e.malId = std::stoi(fields[1]); } catch (...) { e.malId = 0; }
      if (fields.size() > 2 && !fields[2].empty()) {
        try { e.anilistId = std::stoi(fields[2]); } catch (...) { e.anilistId = 0; }
      }

      size_t offset = (fields.size() >= 16) ? 3 : 2;

      e.title = unescapeCsv(fields[offset]);
      e.status = stringToStatus(fields[offset + 1]);
      try { e.score = std::stoi(fields[offset + 2]); } catch (...) { e.score = 0; }
      try { e.progress = std::stoi(fields[offset + 3]); } catch (...) { e.progress = 0; }
      try { e.totalUnits = std::stoi(fields[offset + 4]); } catch (...) { e.totalUnits = 0; }

      if (fields.size() > offset + 5) { try { e.volumes = std::stoi(fields[offset + 5]); } catch (...) {} }
      if (fields.size() > offset + 6) { try { e.totalVolumes = std::stoi(fields[offset + 6]); } catch (...) {} }
      if (fields.size() > offset + 7) { try { e.rewatched = std::stoi(fields[offset + 7]); } catch (...) {} }
      if (fields.size() > offset + 8) e.startDate = unescapeCsv(fields[offset + 8]);
      if (fields.size() > offset + 9) e.finishDate = unescapeCsv(fields[offset + 9]);
      if (fields.size() > offset + 10) e.notes = unescapeCsv(fields[offset + 10]);
      if (fields.size() > offset + 11) e.updatedAt = unescapeCsv(fields[offset + 11]);

      if (e.malId > 0 && e.id != e.malId) {
        e.id = e.malId;
        needsHeal = true;
      }

      list.push_back(e);
    }
  }

  if (needsHeal) {
    saveCsv(type, path);
  }

  return true;
}

bool Database::saveCsv(MediaType type, const std::filesystem::path& path) const {
  std::ofstream file(path);
  if (!file.is_open()) return false;

  file << "#id,mal_id,anilist_id,type,title,status,score,progress,total_units,volumes,total_volumes,rewatched,start_date,finish_date,notes,updated_at,dirty\n";
  for (const auto& e : getList(type)) {
    file << e.id << ","
         << e.malId << ","
         << e.anilistId << ","
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
         << escapeCsv(e.updatedAt) << ","
         << (e.dirty ? "1" : "0") << "\n";
  }
  return true;
}

void Database::logActivity(const ActivityRecord& record) {
  historyRecords_.insert(historyRecords_.begin(), record);
  if (historyRecords_.size() > 100) {
    historyRecords_.resize(100);
  }
  saveHistory();
}

std::vector<ActivityRecord> Database::getRecentActivity(MediaType type, size_t limit) const {
  std::vector<ActivityRecord> res;
  for (const auto& h : historyRecords_) {
    if (h.type == type) {
      res.push_back(h);
      if (res.size() >= limit) break;
    }
  }
  return res;
}

bool Database::loadHistory() {
  auto hPath = getHistoryPath();
  std::ifstream file(hPath);
  if (!file.is_open()) return false;

  historyRecords_.clear();
  std::string line;
  while (std::getline(file, line)) {
    if (line.empty() || line[0] == '#') continue;
    auto fields = parseCsvRecord(line);
    if (fields.size() < 7) continue;

    ActivityRecord r;
    r.timestamp = unescapeCsv(fields[0]);
    r.type = (fields[1] == "anime" ? MediaType::Anime : MediaType::Manga);
    r.title = unescapeCsv(fields[2]);
    r.status = stringToStatus(fields[3]);
    try { r.progress = std::stoi(fields[4]); } catch (...) { r.progress = 0; }
    try { r.totalUnits = std::stoi(fields[5]); } catch (...) { r.totalUnits = 0; }
    try { r.score = std::stoi(fields[6]); } catch (...) { r.score = 0; }
    historyRecords_.push_back(r);
  }
  return true;
}

bool Database::saveHistory() const {
  std::filesystem::create_directories(configDir_);
  auto hPath = getHistoryPath();
  std::ofstream file(hPath);
  if (!file.is_open()) return false;

  file << "#timestamp,type,title,status,progress,total_units,score\n";
  for (const auto& h : historyRecords_) {
    file << escapeCsv(h.timestamp) << ","
         << (h.type == MediaType::Anime ? "anime" : "manga") << ","
         << escapeCsv(h.title) << ","
         << escapeCsv(statusToString(h.status, h.type)) << ","
         << h.progress << ","
         << h.totalUnits << ","
         << h.score << "\n";
  }
  return true;
}

bool Database::loadAll() {
  bool a = loadCsv(MediaType::Anime, getCsvPath(MediaType::Anime));
  bool m = loadCsv(MediaType::Manga, getCsvPath(MediaType::Manga));
  loadHistory();
  autoHealIds();
  return a || m;
}

bool Database::saveAll() const {
  std::filesystem::create_directories(configDir_);
  bool a = saveCsv(MediaType::Anime, getCsvPath(MediaType::Anime));
  bool m = saveCsv(MediaType::Manga, getCsvPath(MediaType::Manga));
  saveHistory();
  return a && m;
}

void Database::seedDefaults() {
  // Purged dummy data instantiation. System now starts perfectly clean.
}

} // namespace catalist
