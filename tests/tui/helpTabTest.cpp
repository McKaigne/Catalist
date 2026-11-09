#include <doctest/doctest.h>

#include "catalist/tui/tabs/helpTab.hpp"
#include "tui/tuiHarness.hpp"

using ftxui::Event;

TEST_CASE("tui help lists tabs and live keybindings") {
  catalist::test::tuiHarness harness;
  harness.config.keybinds.setBinding("moveitemdown", "N");
  catalist::helpTab tab;
  tab.setTabTitles({"Anime", "Manga"});
  auto shown = harness.screen(tab);
  CHECK(shown.find("Go to Manga") != std::string::npos);
  CHECK(shown.find("Queue: move item down") != std::string::npos);
  CHECK(shown.find("  N ") != std::string::npos);
  CHECK(shown.find("Catalist v") != std::string::npos);
  REQUIRE(harness.press(tab, Event::Character('j')));
  REQUIRE(harness.press(tab, Event::Character('G')));
  CHECK_FALSE(harness.press(tab, Event::Character('x')));
}

TEST_CASE("tui aboutLine summarises version counts and data dir") {
  catalist::test::tuiHarness harness;
  auto line = catalist::aboutLine(harness.ctx);
  CHECK(line.find("0 anime · 0 manga · data in /data") != std::string::npos);
}
