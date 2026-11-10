#include "catalist/platform/httpClient.hpp"

#include <array>
#include <cstdio>

#include "catalist/core/shell.hpp"

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
