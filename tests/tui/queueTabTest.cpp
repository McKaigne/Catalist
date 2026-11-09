#include <doctest/doctest.h>

#include "catalist/tui/tabs/queueTab.hpp"
#include "support/sampleLibrary.hpp"
#include "tui/tuiHarness.hpp"

using ftxui::Event;

TEST_CASE("tui queue reorders with J and stores positions") {
  catalist::test::tuiHarness harness;
  catalist::test::seedSampleLibrary(harness.db);
  catalist::queueTab tab;
  tab.onActivate(harness.ctx);

  auto before = catalist::queuedEntries(harness.db);
  REQUIRE(before.size() == 3);
  REQUIRE(harness.press(tab, Event::Character('J')));
  auto after = catalist::queuedEntries(harness.db);
  CHECK(after[0].id == before[1].id);
  CHECK(after[1].id == before[0].id);
  CHECK(after[0].queuePos == 0);

  REQUIRE(harness.press(tab, Event::Character('K')));
  CHECK(catalist::queuedEntries(harness.db)[0].id == before[0].id);
}

TEST_CASE("tui queue steps progress of the selected entry") {
  catalist::test::tuiHarness harness;
  catalist::test::seedSampleLibrary(harness.db);
  catalist::queueTab tab;
  tab.onActivate(harness.ctx);
  auto first = catalist::queuedEntries(harness.db).front();
  REQUIRE(harness.press(tab, Event::Character('+')));
  auto stepped = harness.db.getById(first.type, first.id);
  CHECK(stepped->progress == first.progress + 1);
  CHECK(harness.screen(tab).find("Watch Queue") != std::string::npos);
}
