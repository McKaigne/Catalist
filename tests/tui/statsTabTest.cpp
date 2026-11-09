#include <doctest/doctest.h>

#include "catalist/tui/tabs/statsTab.hpp"
#include "support/sampleLibrary.hpp"
#include "tui/tuiHarness.hpp"

using ftxui::Event;

TEST_CASE("tui stats toggles between anime and manga") {
  catalist::test::tuiHarness harness;
  catalist::test::seedSampleLibrary(harness.db);
  catalist::statsTab tab;
  auto shown = harness.screen(tab);
  CHECK(shown.find("Anime Stats") != std::string::npos);
  CHECK(shown.find("Score Histogram") != std::string::npos);
  CHECK(shown.find("Entries per Start Year") != std::string::npos);

  REQUIRE(harness.press(tab, Event::Character('m')));
  CHECK(harness.screen(tab).find("Manga Stats") != std::string::npos);
  REQUIRE(harness.press(tab, Event::Character('l')));
  CHECK(harness.screen(tab).find("Anime Stats") != std::string::npos);
}
