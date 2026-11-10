#ifndef CATALIST_DATABASE_HPP_
#define CATALIST_DATABASE_HPP_

#include <filesystem>
#include <optional>
#include <vector>

#include "catalist/core/result.hpp"
#include "catalist/model/entry.hpp"
#include "catalist/platform/fileSystem.hpp"

namespace catalist {

enum class sortKey { kScore, kStatus, kTitle, kProgress, kDate };

// The anime and manga lists in memory, and their two CSV files on disk.
class database {
public:
  database(iFileSystem& fileSystem, std::filesystem::path dataDir);

  // Adds a title under a fresh id and returns it. A title that shares a
  // MyAnimeList or AniList id with a stored one is merged into that entry
  // instead (see mergeImported), so importing a list twice adds nothing.
  int addEntry(entry added);
  [[nodiscard]] const std::vector<entry>& getEntries(mediaType type) const;
  [[nodiscard]] std::optional<entry> getById(mediaType type, int id) const;
  [[nodiscard]] std::optional<entry> findByMalId(mediaType type,
                                                 int malId) const;
  bool updateEntry(const entry& changed);
  bool removeEntry(mediaType type, int id);

  // Each key has a natural order: score, progress and date from high to low,
  // title and status from first to last. `reverse` flips it.
  static void sortEntries(std::vector<entry>& entries, sortKey key,
                          bool reverse);

  [[nodiscard]] size_t countByStatus(mediaType type, watchStatus status) const;
  [[nodiscard]] size_t countByScore(mediaType type, int score) const;
  // Over rated entries only; 0 when nothing is rated.
  [[nodiscard]] double averageScore(mediaType type) const;
  [[nodiscard]] int totalProgress(mediaType type) const;
  [[nodiscard]] int totalVolumes(mediaType type) const;
  [[nodiscard]] int totalRewatched(mediaType type) const;
  // An estimate: 24 minutes an episode, 5 minutes a chapter.
  [[nodiscard]] double daysSpent(mediaType type) const;

  // CATALIST_DATA_DIR, then $XDG_CONFIG_HOME/catalist, then
  // ~/.config/catalist.
  [[nodiscard]] static std::filesystem::path getDefaultConfigDir();
  [[nodiscard]] std::filesystem::path getConfigPath() const;
  [[nodiscard]] std::filesystem::path getCsvPath(mediaType type) const;

  // Whether a catalogue file was found; fails, naming the file, when one
  // cannot be read, and then nothing of that file is loaded.
  [[nodiscard]] result<bool> loadAll();
  [[nodiscard]] bool saveAll() const;
  [[nodiscard]] result<bool> loadCsv(mediaType type,
                                     const std::filesystem::path& path);
  [[nodiscard]] bool saveCsv(mediaType type,
                             const std::filesystem::path& path) const;

private:
  std::vector<entry>& getList(mediaType type);
  const std::vector<entry>& getList(mediaType type) const;

  iFileSystem& fileSystem_;
  std::filesystem::path dataDir_;
  std::vector<entry> animeEntries_;
  std::vector<entry> mangaEntries_;
};

}  // namespace catalist

#endif  // CATALIST_DATABASE_HPP_
