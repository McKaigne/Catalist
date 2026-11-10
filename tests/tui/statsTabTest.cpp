#include <doctest/doctest.h>

#include "catalist/tui/tabs/statsTab.hpp"
#include "support/sampleLibrary.hpp"
#include "tui/tuiHarness.hpp"

using ftxui::Event;

TEST_CASE("tui stats shows its cards and toggles between anime and manga") {
  catalist::test::tuiHarness harness;
  catalist::test::seedSampleLibrary(harness.db);
  catalist::statsTab tab;
  auto shown = harness.screen(tab);
  CHECK(shown.find("ANIME OVERVIEW") != std::string::npos);
  CHECK(shown.find("SCORES") != std::string::npos);
  CHECK(shown.find("START YEARS") != std::string::npos);
  CHECK(shown.find("RECENT ACTIVITY") != std::string::npos);
  CHECK(shown.find("Anime (4)") != std::string::npos);

  REQUIRE(harness.press(tab, Event::Character('m')));
  CHECK(harness.screen(tab).find("MANGA OVERVIEW") != std::string::npos);
  REQUIRE(harness.press(tab, Event::Character('k')));
  CHECK(harness.screen(tab).find("ANIME OVERVIEW") != std::string::npos);
}
