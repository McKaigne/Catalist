#pragma once

#include <ftxui/screen/color.hpp>
#include <string>
#include <vector>

namespace catalist {

struct ThemePalette {
  std::string name;
  ftxui::Color base;
  ftxui::Color mantle;
  ftxui::Color crust;
  ftxui::Color surface0;
  ftxui::Color surface1;
  ftxui::Color surface2;
  ftxui::Color text;
  ftxui::Color subtext;
  ftxui::Color overlay0;
  ftxui::Color mauve;
  ftxui::Color lavender;
  ftxui::Color blue;
  ftxui::Color green;
  ftxui::Color yellow;
  ftxui::Color peach;
  ftxui::Color red;
};

class ThemeRegistry {
public:
  static ftxui::Color hexToColor(const std::string& hex);
  static const ThemePalette& getTheme(const std::string& name);
  static bool hasTheme(const std::string& name);
  static std::vector<std::string> getAvailableThemes();
  static void setCustomPalette(const ThemePalette& palette);
  static const ThemePalette& active();
  static void setActive(const std::string& name);

private:
  static ThemePalette activePalette_;
  static ThemePalette customPalette_;
};

} // namespace catalist
