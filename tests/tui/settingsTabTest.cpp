#include <doctest/doctest.h>

#include "catalist/storage/backup.hpp"
#include "catalist/tui/tabs/settingsTab.hpp"
#include "catalist/tui/theme/themeRegistry.hpp"
#include "support/testSupport.hpp"
#include "tui/tuiHarness.hpp"

#include <chrono>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <thread>

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
  CHECK(themeRegistry::active().name != harness.settings.theme);
  tab.onDeactivate(harness.ctx);
  CHECK(themeRegistry::active().name == themeRegistry::getTheme("catppuccin-mocha").name);
}

TEST_CASE("tui settings enter keeps the previewed theme and saves the settings") {
  catalist::test::tuiHarness harness;
  auto tab = makeSettings();
  tab.onActivate(harness.ctx);
  REQUIRE(harness.press(tab, Event::Character('j')));
  REQUIRE(harness.press(tab, Event::Return));
  CHECK(harness.settings.theme != "catppuccin-mocha");
  auto saved = catalist::loadSettings(harness.fs, "/data/settings.json");
  CHECK(saved.theme == harness.settings.theme);
}

TEST_CASE("tui settings preferences toggle sort direction and startup tab") {
  catalist::test::tuiHarness harness;
  auto tab = makeSettings();
  REQUIRE(harness.press(tab, Event::Character('l')));
  REQUIRE(harness.press(tab, Event::Return));
  CHECK_FALSE(harness.settings.mouse);
  REQUIRE(harness.press(tab, Event::Character('j')));
  REQUIRE(harness.press(tab, Event::Return));
  CHECK(harness.settings.startupTab == "manga");
  REQUIRE(harness.press(tab, Event::Character('G')));
  bool wasDescending = harness.settings.reverseSort;
  REQUIRE(harness.press(tab, Event::Return));
  CHECK(harness.settings.reverseSort != wasDescending);
}

namespace {

void waitForJobs(catalist::test::tuiHarness& harness) {
  while (harness.jobs.isRunning())
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
}

std::string popupText(catalist::test::tuiHarness& harness) {
  auto popup = harness.ctx.notices.renderPopup(catalist::themeRegistry::active());
  if (!popup)
    return "";
  auto canvas = ftxui::Screen::Create(ftxui::Dimension::Fixed(160),
                                      ftxui::Dimension::Fixed(20));
  ftxui::Render(canvas, *popup);
  return canvas.ToString();
}

void openCard(catalist::test::tuiHarness& harness, catalist::settingsTab& tab,
              int card) {
  for (int step = 0; step < card; ++step)
    REQUIRE(harness.press(tab, Event::Character('l')));
}

}  // namespace

TEST_CASE("tui settings data card backs up and restores after confirmation") {
  catalist::test::tuiHarness harness;
  harness.db.addEntry(catalist::test::makeEntry().id(1).title("Keep"));
  REQUIRE(harness.db.saveAll());
  auto tab = makeSettings();
  openCard(harness, tab, 3);
  for (int step = 0; step < 3; ++step)
    REQUIRE(harness.press(tab, Event::Character('j')));
  REQUIRE(harness.press(tab, Event::Return));
  auto snapshots = catalist::backup::listSnapshots(harness.fs, "/data");
  REQUIRE(snapshots.size() == 2);

  harness.db.addEntry(catalist::test::makeEntry().id(2).title("Later"));
  REQUIRE(harness.db.saveAll());
  REQUIRE(harness.press(tab, Event::Character('j')));
  REQUIRE(harness.press(tab, Event::Character('j')));
  REQUIRE(harness.press(tab, Event::Return));
  CHECK(tab.capturesInput());
  REQUIRE(harness.press(tab, Event::Return));
  CHECK_FALSE(tab.capturesInput());
  CHECK(harness.db.getEntries(catalist::mediaType::kAnime).size() == 1);
}

TEST_CASE("tui settings data card exports MyAnimeList XML and CSV") {
  catalist::test::tuiHarness harness;
  harness.db.addEntry(catalist::test::makeEntry().id(5).malId(5).title("Five"));
  auto tab = makeSettings();
  openCard(harness, tab, 3);
  REQUIRE(harness.press(tab, Event::Character('j')));
  REQUIRE(harness.press(tab, Event::Return));
  auto xml = harness.fs.readFile("/data/exports/anime_20261020_120000.xml");
  REQUIRE(xml.has_value());
  CHECK(xml->find("<series_animedb_id>5</series_animedb_id>") != std::string::npos);
  harness.ctx.notices.dismiss();
  REQUIRE(harness.press(tab, Event::Character('j')));
  REQUIRE(harness.press(tab, Event::Return));
  CHECK(harness.fs.exists("/data/exports/anime_20261020_120000.csv"));
}

TEST_CASE("tui settings accounts card shows both services and needs a login to sync") {
  catalist::test::tuiHarness harness;
  auto tab = makeSettings();
  openCard(harness, tab, 2);
  auto shown = harness.screen(tab);
  CHECK(shown.find("ACCOUNTS & SYNC") != std::string::npos);
  CHECK(shown.find("MYANIMELIST") != std::string::npos);
  CHECK(shown.find("ANILIST") != std::string::npos);
  CHECK(shown.find("Not connected") != std::string::npos);
  REQUIRE(harness.press(tab, Event::Character('G')));
  REQUIRE(harness.press(tab, Event::Return));
  CHECK_FALSE(harness.jobs.isRunning());
  CHECK(popupText(harness).find("Log in to MyAnimeList or AniList") !=
        std::string::npos);
}

TEST_CASE("tui settings AniList login opens the browser and checks the pasted token") {
  catalist::test::tuiHarness harness;
  harness.http->setResponse("graphql.anilist.co",
                            {200, R"({"data": {"Viewer": {"id": 7, "name": "me"}}})", ""});
  auto tab = makeSettings();
  openCard(harness, tab, 2);
  REQUIRE(harness.press(tab, Event::Character('j')));
  REQUIRE(harness.press(tab, Event::Return));
  REQUIRE(tab.capturesInput());
  REQUIRE_FALSE(harness.launcher.calls().empty());
  CHECK(harness.launcher.calls().back().argv.back().find("client_id=1234") !=
        std::string::npos);
  CHECK(harness.screen(tab).find("paste here") != std::string::npos);

  harness.type(tab, "tok");
  CHECK(harness.screen(tab).find("3 characters") != std::string::npos);
  REQUIRE(harness.press(tab, Event::Return));
  CHECK_FALSE(tab.capturesInput());
  waitForJobs(harness);
  CHECK(harness.fakes.anilistLogin.accessToken() == "tok");
  CHECK(popupText(harness).find("Logged in to AniList as me.") != std::string::npos);
}

TEST_CASE("tui settings AniList login forgets a token AniList rejects") {
  catalist::test::tuiHarness harness;
  harness.http->setResponse(
      "graphql.anilist.co",
      {400, R"({"errors": [{"message": "Invalid token"}], "data": null})", ""});
  auto tab = makeSettings();
  openCard(harness, tab, 2);
  REQUIRE(harness.press(tab, Event::Character('j')));
  REQUIRE(harness.press(tab, Event::Return));
  harness.type(tab, "bad");
  REQUIRE(harness.press(tab, Event::Return));
  waitForJobs(harness);
  CHECK_FALSE(harness.fakes.anilistLogin.isAuthenticated());
  CHECK(popupText(harness).find("AniList rejected the token") != std::string::npos);
}

TEST_CASE("tui settings sync runs in the background when an account is connected") {
  catalist::test::tuiHarness harness;
  REQUIRE(harness.fakes.anilistLogin.storeToken("tok"));
  harness.http->setResponse(
      "graphql.anilist.co",
      {200, R"({"data": {"Viewer": {"id": 7, "name": "me"},
                "MediaListCollection": {"hasNextChunk": false, "lists": [
                  {"isCustomList": false, "entries": [
                    {"id": 1, "status": "CURRENT", "score": 7, "progress": 2,
                     "media": {"id": 30, "idMal": 3, "episodes": 12,
                               "title": {"romaji": "Pulled"}}}]}]}}})", ""});
  auto tab = makeSettings();
  openCard(harness, tab, 2);
  REQUIRE(harness.press(tab, Event::Character('G')));
  REQUIRE(harness.press(tab, Event::Return));
  waitForJobs(harness);
  // The fixture answers the anime and the manga list with the same row.
  CHECK(popupText(harness).find("Sync: 2 added") != std::string::npos);
  CHECK(harness.db.getById(catalist::mediaType::kAnime, 3)->title == "Pulled");
}
