#include <doctest/doctest.h>

#include "catalist/storage/backup.hpp"
#include "catalist/tui/tabs/settingsTab.hpp"
#include "catalist/tui/theme/themeRegistry.hpp"
#include "support/testSupport.hpp"
#include "tui/tuiHarness.hpp"

using catalist::themeRegistry;
using ftxui::Event;

namespace {

catalist::settingsTab makeSettings() {
  return catalist::settingsTab({"anime", "manga", "stats", "settings"});
}

}  // namespace

TEST_CASE("tui settings previews themes and reverts on leave") {
  catalist::test::tuiHarness harness;
  auto tab = makeSettings();
  tab.onActivate(harness.ctx);
  REQUIRE(harness.press(tab, Event::Character('j')));
  CHECK(themeRegistry::active().name != harness.config.theme);
  tab.onDeactivate(harness.ctx);
  CHECK(themeRegistry::active().name == themeRegistry::getTheme("catppuccin-mocha").name);
}

TEST_CASE("tui settings enter keeps the previewed theme and saves the config") {
  catalist::test::tuiHarness harness;
  auto tab = makeSettings();
  tab.onActivate(harness.ctx);
  REQUIRE(harness.press(tab, Event::Character('j')));
  REQUIRE(harness.press(tab, Event::Return));
  CHECK(harness.config.theme != "catppuccin-mocha");
  auto saved = harness.fs.readFile("/data/catalist.kdl");
  REQUIRE(saved.has_value());
  CHECK(saved->find("theme \"" + harness.config.theme + "\"") != std::string::npos);
}

TEST_CASE("tui settings preferences toggle sort direction and startup tab") {
  catalist::test::tuiHarness harness;
  auto tab = makeSettings();
  REQUIRE(harness.press(tab, Event::Character('l')));
  REQUIRE(harness.press(tab, Event::Return));
  CHECK_FALSE(harness.config.mouse);
  REQUIRE(harness.press(tab, Event::Character('j')));
  REQUIRE(harness.press(tab, Event::Return));
  CHECK(harness.config.startupTab == "manga");
  REQUIRE(harness.press(tab, Event::Character('G')));
  bool wasDescending = harness.config.reverseSort;
  REQUIRE(harness.press(tab, Event::Return));
  CHECK(harness.config.reverseSort != wasDescending);
}

TEST_CASE("tui settings data panel backs up and restores after confirmation") {
  catalist::test::tuiHarness harness;
  harness.db.addEntry(catalist::test::makeEntry().id(1).title("Keep"));
  REQUIRE(harness.db.saveAll());
  auto tab = makeSettings();
  REQUIRE(harness.press(tab, Event::Character('l')));
  REQUIRE(harness.press(tab, Event::Character('l')));
  REQUIRE(harness.press(tab, Event::Return));
  auto snapshots = catalist::backup::listSnapshots(harness.fs, "/data");
  REQUIRE(snapshots.size() == 2);

  harness.db.addEntry(catalist::test::makeEntry().id(2).title("Later"));
  REQUIRE(harness.db.saveAll());
  for (int step = 0; step < 6; ++step)
    REQUIRE(harness.press(tab, Event::Character('j')));
  REQUIRE(harness.press(tab, Event::Return));
  CHECK(tab.capturesInput());
  REQUIRE(harness.press(tab, Event::Return));
  CHECK_FALSE(tab.capturesInput());
  CHECK(harness.db.getEntries(catalist::mediaType::kAnime).size() == 1);
}

TEST_CASE("tui settings data panel exports and lists the MAL rows") {
  catalist::test::tuiHarness harness;
  auto tab = makeSettings();
  REQUIRE(harness.press(tab, Event::Character('l')));
  REQUIRE(harness.press(tab, Event::Character('l')));
  REQUIRE(harness.press(tab, Event::Character('j')));
  REQUIRE(harness.press(tab, Event::Character('j')));
  REQUIRE(harness.press(tab, Event::Return));
  CHECK(harness.fs.exists("/data/exports/anime_20261020_120000.csv"));
  auto shown = harness.screen(tab);
  CHECK(shown.find("Sync ongoing titles with MyAnimeList") != std::string::npos);
  CHECK(shown.find("Log in to MyAnimeList") != std::string::npos);
}

TEST_CASE("tui settings data panel starts the metadata sync job") {
  catalist::test::tuiHarness harness;
  auto tab = makeSettings();
  REQUIRE(harness.press(tab, Event::Character('l')));
  REQUIRE(harness.press(tab, Event::Character('l')));
  for (int step = 0; step < 3; ++step)
    REQUIRE(harness.press(tab, Event::Character('j')));
  REQUIRE(harness.press(tab, Event::Return));
  CHECK(harness.notices.status() ==
        "Reconciling ongoing titles with MyAnimeList...");
}
