#pragma once

#include <string>
#include <filesystem>

namespace catalist {

struct AppConfig {
  bool mouse = true;
  std::string theme = "catppuccin-mocha";
  std::string defaultMedia = "anime";
  std::string defaultSort = "score";
  bool reverseSort = true;

  bool load(const std::filesystem::path& path);
  bool save(const std::filesystem::path& path) const;
};

} // namespace catalist
