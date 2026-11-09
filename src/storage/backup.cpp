#include "catalist/storage/backup.hpp"

#include <algorithm>

#include "catalist/storage/csvCodec.hpp"

namespace catalist::backup {

std::filesystem::path backupDirectory(const std::filesystem::path& dataDir) {
  return dataDir / "backups";
}

std::vector<std::filesystem::path> createSnapshot(
    iFileSystem& fileSystem, const iClock& clock,
    const std::filesystem::path& dataDir) {
  auto directory = backupDirectory(dataDir);
  fileSystem.createDirectories(directory);
  std::string stamp = formatLocalTime(clock, "%Y%m%d_%H%M%S");

  std::vector<std::filesystem::path> written;
  for (auto type : {mediaType::kAnime, mediaType::kManga}) {
    auto source = dataDir / csvCodec::catalogFileName(type);
    if (!fileSystem.exists(source))
      continue;
    std::string kind = type == mediaType::kAnime ? "anime" : "manga";
    auto target = directory / (kind + "_" + stamp + ".csv");
    if (fileSystem.copyFile(source, target))
      written.push_back(target);
  }
  return written;
}

std::vector<std::string> listSnapshots(const iFileSystem& fileSystem,
                                       const std::filesystem::path& dataDir) {
  std::vector<std::string> names;
  for (const auto& file : fileSystem.listDirectory(backupDirectory(dataDir)))
    names.push_back(file.filename().string());
  std::sort(names.rbegin(), names.rend());
  return names;
}

}  // namespace catalist::backup
