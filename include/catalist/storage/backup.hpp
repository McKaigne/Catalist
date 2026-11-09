#ifndef CATALIST_STORAGE_BACKUP_HPP_
#define CATALIST_STORAGE_BACKUP_HPP_

#include <filesystem>
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

// Copies backups/<snapshotName> over the catalogue file it was taken from
// (anime_* or manga_*). Returns false for unknown names or failed copies.
[[nodiscard]] bool restoreSnapshot(iFileSystem& fileSystem,
                                   const std::filesystem::path& dataDir,
                                   const std::string& snapshotName);

// Writes exports/<kind>_<YYYYmmdd_HHMMSS>.csv for both catalogues and
// returns the files written.
std::vector<std::filesystem::path> exportCatalog(iFileSystem& fileSystem,
                                                 const iClock& clock,
                                                 const database& db);

}  // namespace catalist::backup

#endif  // CATALIST_STORAGE_BACKUP_HPP_
