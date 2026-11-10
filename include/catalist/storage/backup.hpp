#ifndef CATALIST_STORAGE_BACKUP_HPP_
#define CATALIST_STORAGE_BACKUP_HPP_

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "catalist/core/clock.hpp"
#include "catalist/platform/fileSystem.hpp"
#include "catalist/storage/database.hpp"

namespace catalist::backup {

[[nodiscard]] std::filesystem::path backupDirectory(
    const std::filesystem::path& dataDir);

// Copies the current catalogue files into backups/ as
// <kind>_<YYYYmmdd_HHMMSS>.csv; returns the snapshot files written.
std::vector<std::filesystem::path> createSnapshot(
    iFileSystem& fileSystem, const iClock& clock,
    const std::filesystem::path& dataDir);

// Snapshot file names, newest first.
[[nodiscard]] std::vector<std::string> listSnapshots(
    const iFileSystem& fileSystem, const std::filesystem::path& dataDir);

struct snapshotContent {
  mediaType type;
  std::string csv;
};

// Reads backups/<snapshotName>; the prefix (anime_ or manga_) says which
// catalogue it belongs to. Names with path separators are rejected.
[[nodiscard]] std::optional<snapshotContent> readSnapshot(
    const iFileSystem& fileSystem, const std::filesystem::path& dataDir,
    const std::string& snapshotName);

// Writes previously read snapshot content over its catalogue file.
[[nodiscard]] bool writeSnapshot(iFileSystem& fileSystem,
                                 const std::filesystem::path& dataDir,
                                 const snapshotContent& snapshot);

[[nodiscard]] bool restoreSnapshot(iFileSystem& fileSystem,
                                   const std::filesystem::path& dataDir,
                                   const std::string& snapshotName);

// Writes exports/<kind>_<YYYYmmdd_HHMMSS>.csv for both catalogues and
// returns the files written.
std::vector<std::filesystem::path> exportCatalog(iFileSystem& fileSystem,
                                                 const iClock& clock,
                                                 const database& db);

struct xmlExportResult {
  std::vector<std::filesystem::path> files;
  size_t skipped = 0;
};

// Writes exports/<kind>_<YYYYmmdd_HHMMSS>.xml in the MyAnimeList format for
// both catalogues; entries without a MyAnimeList id are counted as skipped.
xmlExportResult exportMalXml(iFileSystem& fileSystem, const iClock& clock,
                             const database& db);

}  // namespace catalist::backup

#endif  // CATALIST_STORAGE_BACKUP_HPP_
