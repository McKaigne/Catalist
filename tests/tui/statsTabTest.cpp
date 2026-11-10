#include <doctest/doctest.h>

#include "catalist/tui/tabs/statsTab.hpp"
#include "support/sampleLibrary.hpp"
#include "tui/tuiHarness.hpp"

using ftxui::Event;

TEST_CASE("tui stats shows the totals of one media type at a time") {
  catalist::test::tuiHarness harness;
  catalist::test::seedSampleLibrary(harness.db);
  catalist::statsTab tab;
  auto shown = harness.screen(tab);
  CHECK(shown.find("ANIME OVERVIEW") != std::string::npos);
  CHECK(shown.find("SCORES") != std::string::npos);
  CHECK(shown.find("Anime (4)") != std::string::npos);
  CHECK(shown.find("Manga (2)") != std::string::npos);
  // 36 episodes at 24 minutes; mean of the rated 10, 8 and 6.
  CHECK(shown.find("Episodes:") != std::string::npos);
  CHECK(shown.find("36") != std::string::npos);
  CHECK(shown.find("0.6") != std::string::npos);
  CHECK(shown.find("8.00") != std::string::npos);
  CHECK(shown.find("Plan to Watch") != std::string::npos);
  CHECK(shown.find("RECENT ACTIVITY") == std::string::npos);
  CHECK(shown.find("START YEARS") == std::string::npos);

  REQUIRE(harness.press(tab, Event::Character('m')));
  shown = harness.screen(tab);
  CHECK(shown.find("MANGA OVERVIEW") != std::string::npos);
  // 461 chapters at 5 minutes; 42 + 11 volumes; mean of 10 and 9.
  CHECK(shown.find("Chapters:") != std::string::npos);
  CHECK(shown.find("461") != std::string::npos);
  CHECK(shown.find("1.6") != std::string::npos);
  CHECK(shown.find("53") != std::string::npos);
  CHECK(shown.find("9.50") != std::string::npos);
  CHECK(shown.find("Plan to Read") != std::string::npos);

  REQUIRE(harness.press(tab, Event::Character('k')));
  CHECK(harness.screen(tab).find("ANIME OVERVIEW") != std::string::npos);
  CHECK_FALSE(harness.press(tab, Event::Character('x')));
}
