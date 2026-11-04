#pragma once

#include <filesystem>
#include <string>

#include "catalist/kdl.hpp"
#include "catalist/keybindManager.hpp"
#include "catalist/themeRegistry.hpp"

namespace catalist {

struct AppConfig {
  bool mouse = true;
  std::string theme = "catppuccin-mocha";
  std::string defaultMedia = "anime";
  std::string defaultSort = "score";
  bool reverseSort = true;

  KeybindManager keybinds;
  ThemePalette customPalette;
  kdl::Document kdlDoc;

  bool load(const std::filesystem::path& path);
  bool save(const std::filesystem::path& path) const;
  bool generateDefault(const std::filesystem::path& path);
};

}  // namespace catalist
