#ifndef CATALIST_SETTINGS_SETTINGS_HPP_
#define CATALIST_SETTINGS_SETTINGS_HPP_

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "catalist/platform/fileSystem.hpp"

namespace catalist {

// The preferences the Settings tab and `catalist config` change, stored by the
// app in settings.json; users are not expected to edit that file.
struct appSettings {
  bool mouse = true;
  std::string theme = "catppuccin-mocha";
  std::string defaultMedia = "anime";
  std::string defaultSort = "score";
  bool reverseSort = true;
  // Tab id shown at launch; defaultMedia's tab when unset.
  std::optional<std::string> startupTab;
  bool welcomeShown = false;
};

[[nodiscard]] const std::vector<std::string>& mediaChoices();
[[nodiscard]] const std::vector<std::string>& sortChoices();
// Ids of the TUI tabs, in top-bar order.
[[nodiscard]] const std::vector<std::string>& tabChoices();

// Missing or unreadable files and invalid values fall back to the defaults.
[[nodiscard]] appSettings loadSettings(const iFileSystem& fileSystem,
                                       const std::filesystem::path& path);
bool saveSettings(iFileSystem& fileSystem, const std::filesystem::path& path,
                  const appSettings& settings);

}  // namespace catalist

#endif  // CATALIST_SETTINGS_SETTINGS_HPP_
