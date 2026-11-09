#include <doctest/doctest.h>

#include "catalist/tui/tabs/historyTab.hpp"
#include "tui/tuiHarness.hpp"

using catalist::mediaType;
using catalist::watchStatus;
using ftxui::Event;

TEST_CASE("tui history groups days and moves between them") {
  catalist::test::tuiHarness harness;
  harness.db.logActivity({"2026-10-19 08:00", mediaType::kAnime, "Older",
                          watchStatus::kWatching, 1, 2, 0});
  harness.db.logActivity({"2026-10-20 09:30", mediaType::kManga, "Newer",
                          watchStatus::kCompleted, 3, 3, 8});
  catalist::historyTab tab;
  tab.onActivate(harness.ctx);

  auto shown = harness.screen(tab);
  CHECK(shown.find("2026-10-20 Tue") != std::string::npos);
  CHECK(shown.find("09:30") != std::string::npos);
  CHECK(shown.find("Newer") != std::string::npos);

  REQUIRE(harness.press(tab, Event::Character('j')));
  CHECK(harness.screen(tab).find("Older") != std::string::npos);
  CHECK_FALSE(harness.press(tab, Event::Character('x')));
}
