#include "catalist/storage/backup.hpp"

#include <algorithm>
#include <optional>

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

std::optional<snapshotContent> readSnapshot(
    const iFileSystem& fileSystem, const std::filesystem::path& dataDir,
    const std::string& snapshotName) {
  bool isPlainName = snapshotName.find('/') == std::string::npos &&
                     snapshotName.find("..") == std::string::npos;
  if (!isPlainName)
    return std::nullopt;
  std::optional<mediaType> kind;
  if (snapshotName.rfind("anime_", 0) == 0)
    kind = mediaType::kAnime;
  else if (snapshotName.rfind("manga_", 0) == 0)
    kind = mediaType::kManga;
  if (!kind)
    return std::nullopt;
  auto csv = fileSystem.readFile(backupDirectory(dataDir) / snapshotName);
  if (!csv)
    return std::nullopt;
  return snapshotContent{*kind, std::move(*csv)};
}

bool writeSnapshot(iFileSystem& fileSystem, const std::filesystem::path& dataDir,
                   const snapshotContent& snapshot) {
  return fileSystem.writeFileAtomic(
      dataDir / csvCodec::catalogFileName(snapshot.type), snapshot.csv);
}

bool restoreSnapshot(iFileSystem& fileSystem,
                     const std::filesystem::path& dataDir,
                     const std::string& snapshotName) {
  auto snapshot = readSnapshot(fileSystem, dataDir, snapshotName);
  return snapshot && writeSnapshot(fileSystem, dataDir, *snapshot);
}

std::vector<std::filesystem::path> exportCatalog(iFileSystem& fileSystem,
                                                 const iClock& clock,
                                                 const database& db) {
  auto directory = db.getConfigPath() / "exports";
  fileSystem.createDirectories(directory);
  std::string stamp = formatLocalTime(clock, "%Y%m%d_%H%M%S");
  std::vector<std::filesystem::path> written;
  for (auto type : {mediaType::kAnime, mediaType::kManga}) {
    std::string kind = type == mediaType::kAnime ? "anime" : "manga";
    auto target = directory / (kind + "_" + stamp + ".csv");
    if (fileSystem.writeFileAtomic(target,
                                   csvCodec::encodeEntries(db.getEntries(type))))
      written.push_back(target);
  }
  return written;
}

}  // namespace catalist::backup
