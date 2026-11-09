#include <doctest/doctest.h>

#include "catalist/tui/tabs/libraryTab.hpp"
#include "support/sampleLibrary.hpp"
#include "tui/tuiHarness.hpp"

using ftxui::Event;

TEST_CASE("tui library walks year, status and entries columns") {
  catalist::test::tuiHarness harness;
  catalist::test::seedSampleLibrary(harness.db);
  catalist::libraryTab tab;
  tab.onActivate(harness.ctx);

  auto shown = harness.screen(tab);
  CHECK(shown.find("2023") != std::string::npos);
  CHECK(shown.find("No date") != std::string::npos);

  REQUIRE(harness.press(tab, Event::Character('l')));
  REQUIRE(harness.press(tab, Event::Character('j')));
  CHECK(harness.screen(tab).find("Sousou no Frieren") != std::string::npos);
  REQUIRE(harness.press(tab, Event::Character('G')));
  REQUIRE(harness.press(tab, Event::Character('g')));
  CHECK_FALSE(harness.press(tab, Event::Character('z')));
}

TEST_CASE("tui library toggles to manga") {
  catalist::test::tuiHarness harness;
  catalist::test::seedSampleLibrary(harness.db);
  catalist::libraryTab tab;
  tab.onActivate(harness.ctx);
  REQUIRE(harness.press(tab, Event::Character('m')));
  auto shown = harness.screen(tab);
  CHECK(shown.find("Manga · Start Year") != std::string::npos);
  CHECK(shown.find("2019") != std::string::npos);
}
