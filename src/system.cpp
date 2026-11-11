#include "system.hpp"

#include <array>
#include <cstdio>
#include <ctime>
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

namespace catalist {

iClock::timePoint realClock::now() const {
  return std::chrono::system_clock::now();
}

isoDate localDate(const iClock& clock) {
  std::time_t seconds = std::chrono::system_clock::to_time_t(clock.now());
  std::tm local{};
  localtime_r(&seconds, &local);
  return isoDate::fromLocalTm(local);
}

}  // namespace catalist

namespace catalist {

std::optional<std::string> curlHttpClient::get(const std::string& url) {
  // -f turns an HTTP error into a failed exit, -m bounds the whole request so
  // the interface never hangs longer than that.
  std::string command = "curl -sfL -m 8 -A catalist " +
                        shell::escapeShellArg(url) + " 2>/dev/null";
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

}  // namespace catalist

namespace catalist::shell {

std::string escapeShellArg(std::string_view arg) {
  std::string quoted = "'";
  for (char c : arg) {
    if (c == '\'')
      quoted += "'\\''";
    else
      quoted += c;
  }
  quoted += "'";
  return quoted;
}

}  // namespace catalist::shell
