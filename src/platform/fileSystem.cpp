#include "catalist/platform/fileSystem.hpp"

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
  std::error_code ec;
  if (!file.parent_path().empty())
    std::filesystem::create_directories(file.parent_path(), ec);
  path tempFile = file.string() + ".tmp";
  bool written = false;
  {
    std::ofstream stream(tempFile, std::ios::binary | std::ios::trunc);
    stream << content;
    stream.flush();
    written = stream.is_open() && stream.good();
  }
  if (written)
    std::filesystem::rename(tempFile, file, ec);
  if (written && !ec)
    return true;
  std::filesystem::remove(tempFile, ec);
  return false;
}

}  // namespace catalist
