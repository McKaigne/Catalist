#ifndef CATALIST_CONFIG_HPP_
#define CATALIST_CONFIG_HPP_

#include <filesystem>
#include <optional>
#include <string>

#include "catalist/config/kdl.hpp"
#include "catalist/platform/fileSystem.hpp"
#include "catalist/config/keybindManager.hpp"

namespace catalist {

struct appConfig {
  bool mouse = true;
  std::string theme = "catppuccin-mocha";
  std::string defaultMedia = "anime";
  std::string defaultSort = "score";
  bool reverseSort = true;
  // Tab id shown at launch; falls back to defaultMedia when unset.
  std::optional<std::string> startupTab;
  bool welcomeShown = false;

  keybindManager keybinds;
  kdl::document kdlDoc;

  bool load(iFileSystem& fileSystem, const std::filesystem::path& path);
  bool save(iFileSystem& fileSystem, const std::filesystem::path& path) const;
  bool generateDefault(iFileSystem& fileSystem,
                       const std::filesystem::path& path);
};

}  // namespace catalist

#endif  // CATALIST_CONFIG_HPP_
