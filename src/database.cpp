#include "catalist/database.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <chrono>
#include <iomanip>

namespace catalist {

Database::Database() = default;

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

std::filesystem::path Database::getConfigDir() {
  const char* xdg = std::getenv("XDG_CONFIG_HOME");
  if (xdg && *xdg) return std::filesystem::path(xdg) / "catalist";

  const char* home = std::getenv("HOME");
  if (home && *home) return std::filesystem::path(home) / ".config" / "catalist";

  const char* appdata = std::getenv("APPDATA");
  if (appdata && *appdata) return std::filesystem::path(appdata) / "catalist";

  return std::filesystem::path("./config");
}

std::filesystem::path Database::getCsvPath(MediaType type) const {
  return getConfigDir() / (type == MediaType::Anime ? "anime.csv" : "manga.csv");
}

std::filesystem::path Database::getBackupDir() const {
  return getConfigDir() / "backups";
}

void Database::createBackupSnapshot() {
  auto bdir = getBackupDir();
  std::filesystem::create_directories(bdir);

  auto now = std::chrono::system_clock::now();
  auto in_time_t = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
  ss << std::put_time(std::localtime(&in_time_t), "%Y%m%d_%H%M%S");
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

static std::vector<std::string> parseCsvLine(const std::string& line) {
  std::vector<std::string> fields;
  std::string field;
  bool inQuotes = false;

  for (size_t i = 0; i < line.size(); ++i) {
    char c = line[i];
    if (inQuotes) {
      if (c == '"') {
        if (i + 1 < line.size() && line[i + 1] == '"') {
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
  bool needsHeal = false;

  while (std::getline(file, line)) {
    if (line.empty() || line[0] == '#') continue;

    auto fields = parseCsvLine(line);
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

  if (needsHeal) {
    saveCsv(type, path);
  }

  return true;
}

bool Database::saveCsv(MediaType type, const std::filesystem::path& path) const {
  std::ofstream file(path);
  if (!file.is_open()) return false;

  file << "#id,mal_id,anilist_id,title,status,score,progress,total_units,volumes,total_volumes,rewatched,start_date,finish_date,notes,updated_at,dirty\n";
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

bool Database::loadAll() {
  bool a = loadCsv(MediaType::Anime, getCsvPath(MediaType::Anime));
  bool m = loadCsv(MediaType::Manga, getCsvPath(MediaType::Manga));
  autoHealIds();
  return a || m;
}

bool Database::saveAll() const {
  std::filesystem::create_directories(getConfigDir());
  bool a = saveCsv(MediaType::Anime, getCsvPath(MediaType::Anime));
  bool m = saveCsv(MediaType::Manga, getCsvPath(MediaType::Manga));
  return a && m;
}

void Database::seedDefaults() {
  addEntry({52991, 52991, 154587, "Sousou no Frieren", MediaType::Anime, WatchStatus::Completed, 10, 28, 28, 0, 0, 1, "2023-09-29", "2024-03-22", "Masterpiece adaptation.", "2026-10-17", false});
  addEntry({49387, 49387, 136430, "Vinland Saga Season 2", MediaType::Anime, WatchStatus::Completed, 9, 24, 24, 0, 0, 0, "2023-01-10", "2023-06-20", "Farmland arc.", "2026-10-17", false});
  addEntry({457, 457, 457, "Mushishi", MediaType::Anime, WatchStatus::Completed, 9, 26, 26, 0, 0, 0, "0000-00-00", "0000-00-00", "Atmospheric.", "2026-10-17", false});
  addEntry({44265, 44265, 128547, "Odd Taxi", MediaType::Anime, WatchStatus::Completed, 8, 13, 13, 0, 0, 0, "2021-04-06", "2021-06-29", "Original screenplay.", "2026-10-17", false});

  addEntry({2, 2, 30002, "Berserk", MediaType::Manga, WatchStatus::Watching, 10, 364, 0, 42, 0, 0, "0000-00-00", "0000-00-00", "Dark fantasy pinnacle.", "2026-10-17", false});
  addEntry({116778, 116778, 105778, "Chainsaw Man", MediaType::Manga, WatchStatus::Watching, 9, 217, 0, 18, 0, 0, "0000-00-00", "0000-00-00", "Part 2 ongoing.", "2026-10-17", false});
  addEntry({135496, 135496, 132049, "Dandadan", MediaType::Manga, WatchStatus::Watching, 9, 167, 0, 16, 0, 0, "0000-00-00", "0000-00-00", "Peak art.", "2026-10-17", false});
  addEntry({126146, 126146, 117195, "[Oshi no Ko]", MediaType::Manga, WatchStatus::Completed, 8, 166, 166, 16, 16, 0, "0000-00-00", "2024-11-14", "Entertainment industry.", "2026-10-17", false});
}

} // namespace catalist
