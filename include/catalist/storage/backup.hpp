#ifndef CATALIST_STORAGE_BACKUP_HPP_
#define CATALIST_STORAGE_BACKUP_HPP_

#include <filesystem>
#include <string>
#include <vector>

#include "catalist/core/clock.hpp"
#include "catalist/platform/fileSystem.hpp"

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

}  // namespace catalist::backup

#endif  // CATALIST_STORAGE_BACKUP_HPP_
