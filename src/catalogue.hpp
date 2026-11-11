#ifndef CATALIST_CATALOGUE_HPP_
#define CATALIST_CATALOGUE_HPP_

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "date.hpp"
#include "entry.hpp"
#include "result.hpp"

// RFC 4180 encoding and decoding of the catalogue files. Pure text in,
// records out; reading and writing the files is the catalogue's job.
namespace catalist::csvCodec {

[[nodiscard]] std::string catalogFileName(mediaType type);

[[nodiscard]] std::string escapeField(const std::string& field);

// Splits text into records, joining quoted fields that span lines. Blank
// lines and '#' comment lines between records are skipped; CRLF is accepted.
[[nodiscard]] std::vector<std::string> splitRecords(std::string_view text);
[[nodiscard]] std::vector<std::string> splitFields(std::string_view record);

[[nodiscard]] std::optional<isoDate> decodeDate(const std::string& field);

// The 14 columns of encodeEntries, or the 16 that Catalist 0.2.1 to 0.3.0
// wrote (their AniList id and notes are dropped). Columns after them are
// ignored; a row without the type column where either layout has it is in an
// older layout and is not read.
[[nodiscard]] std::optional<entry> decodeEntry(
    const std::vector<std::string>& fields, mediaType type);
// Fails on the first row decodeEntry cannot read, so a file is never loaded
// half-read.
[[nodiscard]] result<std::vector<entry>> decodeEntries(std::string_view text,
                                                       mediaType type);
[[nodiscard]] std::string encodeEntries(const std::vector<entry>& entries);

}  // namespace catalist::csvCodec

namespace catalist {

enum class sortKey { kScore, kStatus, kTitle, kProgress, kDate };

// The anime and manga lists in memory, and their two CSV files on disk.
class catalogue {
public:
  explicit catalogue(std::filesystem::path dataDir);

  // Adds a title under a fresh id and returns it. A title that shares its
  // MyAnimeList id with a stored one is merged into that entry instead (see
  // mergeImported), so importing a list twice adds nothing.
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

  std::filesystem::path dataDir_;
  std::vector<entry> animeEntries_;
  std::vector<entry> mangaEntries_;
};

}  // namespace catalist

#endif  // CATALIST_CATALOGUE_HPP_
