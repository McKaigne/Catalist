#ifndef CATALIST_CONFIG_HPP_
#define CATALIST_CONFIG_HPP_

#include <filesystem>
#include <string>

#include "catalist/kdl.hpp"
#include "catalist/keybindManager.hpp"

namespace catalist {

struct appConfig {
  bool mouse = true;
  std::string theme = "catppuccin-mocha";
  std::string defaultMedia = "anime";
  std::string defaultSort = "score";
  bool reverseSort = true;

  keybindManager keybinds;
  kdl::document kdlDoc;

  bool load(const std::filesystem::path& path);
  bool save(const std::filesystem::path& path) const;
  bool generateDefault(const std::filesystem::path& path);
};

}  // namespace catalist

#endif  // CATALIST_CONFIG_HPP_
