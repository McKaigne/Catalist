#include "catalist/platform/fileSystem.hpp"

#include <sys/stat.h>

#include <fstream>
#include <iterator>

namespace catalist {

std::optional<std::string> realFileSystem::readFile(const path& file) const {
  std::ifstream stream(file, std::ios::binary);
  if (!stream.is_open())
    return std::nullopt;
  return std::string((std::istreambuf_iterator<char>(stream)),
                     std::istreambuf_iterator<char>());
}

bool realFileSystem::writeFileAtomic(const path& file,
                                     const std::string& content) {
  if (!file.parent_path().empty())
    createDirectories(file.parent_path());
  path tempFile = file.string() + ".tmp";
  {
    std::ofstream stream(tempFile, std::ios::binary | std::ios::trunc);
    if (!stream.is_open())
      return false;
    stream << content;
    if (!stream.good())
      return false;
  }
  std::error_code ec;
  std::filesystem::rename(tempFile, file, ec);
  return !ec;
}

bool realFileSystem::appendFile(const path& file, const std::string& content) {
  if (!file.parent_path().empty())
    createDirectories(file.parent_path());
  std::ofstream stream(file, std::ios::binary | std::ios::app);
  if (!stream.is_open())
    return false;
  stream << content;
  return stream.good();
}

bool realFileSystem::exists(const path& target) const {
  std::error_code ec;
  return std::filesystem::exists(target, ec);
}

bool realFileSystem::createDirectories(const path& directory) {
  std::error_code ec;
  std::filesystem::create_directories(directory, ec);
  return !ec;
}

bool realFileSystem::copyFile(const path& from, const path& to) {
  std::error_code ec;
  std::filesystem::copy_file(
      from, to, std::filesystem::copy_options::overwrite_existing, ec);
  return !ec;
}

bool realFileSystem::removeFile(const path& file) {
  std::error_code ec;
  return std::filesystem::remove(file, ec);
}

std::vector<iFileSystem::path> realFileSystem::listDirectory(
    const path& directory) const {
  std::vector<path> entries;
  std::error_code ec;
  for (const auto& item : std::filesystem::directory_iterator(directory, ec)) {
    if (item.is_regular_file(ec))
      entries.push_back(item.path());
  }
  return entries;
}

bool realFileSystem::restrictToOwner(const path& file) {
  return chmod(file.c_str(), S_IRUSR | S_IWUSR) == 0;
}

std::string memoryFileSystem::key(const path& target) {
  return target.lexically_normal().generic_string();
}

std::optional<std::string> memoryFileSystem::readFile(const path& file) const {
  auto found = files_.find(key(file));
  if (found == files_.end())
    return std::nullopt;
  return found->second;
}

bool memoryFileSystem::writeFileAtomic(const path& file,
                                       const std::string& content) {
  if (failingWrites_.count(key(file)))
    return false;
  files_[key(file)] = content;
  return true;
}

bool memoryFileSystem::appendFile(const path& file,
                                  const std::string& content) {
  if (failingWrites_.count(key(file)))
    return false;
  files_[key(file)] += content;
  return true;
}

bool memoryFileSystem::exists(const path& target) const {
  return files_.count(key(target)) > 0 || directories_.count(key(target)) > 0;
}

bool memoryFileSystem::createDirectories(const path& directory) {
  for (path current = directory; !current.empty() && current != current.parent_path();
       current = current.parent_path())
    directories_.insert(key(current));
  return true;
}

bool memoryFileSystem::copyFile(const path& from, const path& to) {
  auto source = readFile(from);
  if (!source)
    return false;
  return writeFileAtomic(to, *source);
}

bool memoryFileSystem::removeFile(const path& file) {
  return files_.erase(key(file)) > 0;
}

std::vector<iFileSystem::path> memoryFileSystem::listDirectory(
    const path& directory) const {
  std::vector<path> entries;
  std::string parent = key(directory);
  for (const auto& [name, content] : files_) {
    if (path(name).parent_path().generic_string() == parent)
      entries.emplace_back(name);
  }
  return entries;
}

bool memoryFileSystem::restrictToOwner(const path& file) {
  ownerOnly_.insert(key(file));
  return true;
}

bool memoryFileSystem::isOwnerOnly(const path& file) const {
  return ownerOnly_.count(key(file)) > 0;
}

}  // namespace catalist
