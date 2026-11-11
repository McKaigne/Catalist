#include "system.hpp"

#include <array>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iterator>

namespace catalist {

std::optional<std::string> readFile(const std::filesystem::path& file) {
  // A directory opens as a stream on Linux but throws when read.
  std::error_code ec;
  if (!std::filesystem::is_regular_file(file, ec))
    return std::nullopt;
  std::ifstream stream(file, std::ios::binary);
  if (!stream.is_open())
    return std::nullopt;
  return std::string((std::istreambuf_iterator<char>(stream)),
                     std::istreambuf_iterator<char>());
}

bool writeFileAtomic(const std::filesystem::path& file,
                     const std::string& content) {
  std::error_code ec;
  if (!file.parent_path().empty())
    std::filesystem::create_directories(file.parent_path(), ec);
  std::filesystem::path tempFile = file.string() + ".tmp";
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

std::optional<std::string> httpGet(const std::string& url) {
  // -f turns an HTTP error into a failed exit, -m bounds the whole request so
  // the interface never hangs longer than that.
  std::string command =
      "curl -sfL -m 8 -A catalist " + shellQuote(url) + " 2>/dev/null";
  FILE* pipe = popen(command.c_str(), "r");
  if (!pipe)
    return std::nullopt;

  std::string body;
  std::array<char, 4096> buffer;
  size_t count = 0;
  while ((count = fread(buffer.data(), 1, buffer.size(), pipe)) > 0)
    body.append(buffer.data(), count);
  return pclose(pipe) == 0 ? std::optional<std::string>(body) : std::nullopt;
}

isoDate today() {
  std::time_t seconds =
      std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  std::tm local{};
  localtime_r(&seconds, &local);
  return isoDate::fromLocalTm(local);
}

std::string shellQuote(std::string_view text) {
  std::string quoted = "'";
  for (char c : text) {
    if (c == '\'')
      quoted += "'\\''";
    else
      quoted += c;
  }
  return quoted + "'";
}

}  // namespace catalist
