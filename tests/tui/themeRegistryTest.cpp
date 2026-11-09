#include <doctest/doctest.h>

#include <algorithm>

#include "catalist/tui/theme/themeRegistry.hpp"

using catalist::themeRegistry;

TEST_CASE("tui themeRegistry lists and activates known themes") {
  auto themes = themeRegistry::getAvailableThemes();
  CHECK(std::find(themes.begin(), themes.end(), "nord") != themes.end());
  CHECK(themeRegistry::hasTheme("dracula"));
  CHECK_FALSE(themeRegistry::hasTheme("no-such-theme"));

  themeRegistry::setActive("nord");
  CHECK(themeRegistry::active().name == themeRegistry::getTheme("nord").name);
}

TEST_CASE("tui themeRegistry falls back for unknown names") {
  themeRegistry::setActive("gruvbox");
  themeRegistry::setActive("no-such-theme");
  CHECK_FALSE(themeRegistry::active().name.empty());
}

TEST_CASE("tui themeRegistry parses hex colours") {
  CHECK(themeRegistry::hexToColor("#ff0000") == ftxui::Color::RGB(255, 0, 0));
  CHECK(themeRegistry::hexToColor("00ff00") == ftxui::Color::RGB(0, 255, 0));
}
