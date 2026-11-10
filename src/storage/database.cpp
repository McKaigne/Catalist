#include "catalist/storage/database.hpp"

#include <algorithm>
#include <cstdlib>
#include <utility>

#include "catalist/model/entryOps.hpp"
#include "catalist/storage/csvCodec.hpp"

namespace catalist {

namespace {

// A local id is only a row number, so two titles are the same only when a
// list service says so.
bool sharesRemoteId(const entry& existing, const entry& incoming) {
  if (incoming.malId > 0 && existing.malId == incoming.malId)
    return true;
  return incoming.anilistId > 0 && existing.anilistId == incoming.anilistId;
}

int nextId(const std::vector<entry>& list) {
  int highest = 0;
  for (const auto& e : list)
    highest = std::max(highest, e.id);
  return highest + 1;
}

// True when `a` comes before `b` in the natural order of `key`; ties fall
// back to the title so the order never depends on how the list was stored.
bool comesBefore(const entry& a, const entry& b, sortKey key) {
  switch (key) {
    case sortKey::kScore:
      if (a.score != b.score)
        return a.score > b.score;
      break;
    case sortKey::kProgress:
      if (a.progress != b.progress)
        return a.progress > b.progress;
      break;
    case sortKey::kDate:
      if (listDate(a) != listDate(b))
        return listDate(a) > listDate(b);
      break;
    case sortKey::kStatus:
      if (a.status != b.status)
        return static_cast<int>(a.status) < static_cast<int>(b.status);
      break;
    case sortKey::kTitle:
      break;
  }
  return a.title < b.title;
}

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

  return std::filesystem::path("catalist-data");
}

database::database(iFileSystem& fileSystem, std::filesystem::path dataDir)
    : fileSystem_(fileSystem), dataDir_(std::move(dataDir)) {}

std::filesystem::path database::getConfigPath() const {
  return dataDir_;
}

std::filesystem::path database::getCsvPath(mediaType type) const {
  return dataDir_ / csvCodec::catalogFileName(type);
}

std::vector<entry>& database::getList(mediaType type) {
  return type == mediaType::kAnime ? animeEntries_ : mangaEntries_;
}

const std::vector<entry>& database::getList(mediaType type) const {
  return type == mediaType::kAnime ? animeEntries_ : mangaEntries_;
}

void database::sortEntries(std::vector<entry>& entries, sortKey key,
                           bool reverse) {
  std::sort(entries.begin(), entries.end(),
            [key, reverse](const entry& a, const entry& b) {
              return reverse ? comesBefore(b, a, key) : comesBefore(a, b, key);
            });
}

int database::addEntry(entry added) {
  auto& list = getList(added.type);
  for (auto& existing : list) {
    if (!sharesRemoteId(existing, added))
      continue;
    existing = mergeImported(existing, added);
    return existing.id;
  }
  added.id = nextId(list);
  list.push_back(std::move(added));
  return list.back().id;
}

const std::vector<entry>& database::getEntries(mediaType type) const {
  return getList(type);
}

std::optional<entry> database::getById(mediaType type, int id) const {
  for (const auto& e : getList(type)) {
    if (e.id == id)
      return e;
  }
  return std::nullopt;
}

std::optional<entry> database::findByMalId(mediaType type, int malId) const {
  for (const auto& e : getList(type)) {
    if (malId > 0 && e.malId == malId)
      return e;
  }
  return std::nullopt;
}

bool database::updateEntry(const entry& changed) {
  for (auto& e : getList(changed.type)) {
    if (e.id == changed.id) {
      e = changed;
      return true;
    }
  }
  return false;
}

bool database::removeEntry(mediaType type, int id) {
  auto& list = getList(type);
  auto found = std::find_if(list.begin(), list.end(),
                            [id](const entry& e) { return e.id == id; });
  if (found == list.end())
    return false;
  list.erase(found);
  return true;
}

size_t database::countByStatus(mediaType type, watchStatus status) const {
  const auto& list = getList(type);
  return static_cast<size_t>(
      std::count_if(list.begin(), list.end(),
                    [status](const entry& e) { return e.status == status; }));
}

size_t database::countByScore(mediaType type, int score) const {
  const auto& list = getList(type);
  return static_cast<size_t>(
      std::count_if(list.begin(), list.end(),
                    [score](const entry& e) { return e.score == score; }));
}

double database::averageScore(mediaType type) const {
  int sum = 0;
  int rated = 0;
  for (const auto& e : getList(type)) {
    if (e.score > 0) {
      sum += e.score;
      ++rated;
    }
  }
  return rated == 0 ? 0.0 : static_cast<double>(sum) / rated;
}

int database::totalProgress(mediaType type) const {
  int total = 0;
  for (const auto& e : getList(type))
    total += e.progress;
  return total;
}

int database::totalVolumes(mediaType type) const {
  int total = 0;
  for (const auto& e : getList(type))
    total += e.volumes;
  return total;
}

int database::totalRewatched(mediaType type) const {
  int total = 0;
  for (const auto& e : getList(type))
    total += e.rewatched;
  return total;
}

double database::daysSpent(mediaType type) const {
  constexpr double kMinutesPerDay = 1440.0;
  double minutesPerUnit = type == mediaType::kAnime ? 24.0 : 5.0;
  return totalProgress(type) * minutesPerUnit / kMinutesPerDay;
}

result<bool> database::loadCsv(mediaType type,
                               const std::filesystem::path& path) {
  auto content = fileSystem_.readFile(path);
  if (!content)
    return result<bool>::success(false);
  auto decoded = csvCodec::decodeEntries(*content, type);
  if (!decoded.ok())
    return result<bool>::failure(
        path.string() + ": " + decoded.errorMessage() +
        ". Open and save it once with Catalist 0.3.0 to convert it.");
  getList(type) = std::move(decoded.value());
  return result<bool>::success(true);
}

bool database::saveCsv(mediaType type,
                       const std::filesystem::path& path) const {
  return fileSystem_.writeFileAtomic(path,
                                     csvCodec::encodeEntries(getList(type)));
}

result<bool> database::loadAll() {
  auto anime = loadCsv(mediaType::kAnime, getCsvPath(mediaType::kAnime));
  if (!anime.ok())
    return anime;
  auto manga = loadCsv(mediaType::kManga, getCsvPath(mediaType::kManga));
  if (!manga.ok())
    return manga;
  return result<bool>::success(anime.value() || manga.value());
}

bool database::saveAll() const {
  bool anime = saveCsv(mediaType::kAnime, getCsvPath(mediaType::kAnime));
  bool manga = saveCsv(mediaType::kManga, getCsvPath(mediaType::kManga));
  return anime && manga;
}

}  // namespace catalist
