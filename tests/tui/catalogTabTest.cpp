#include <doctest/doctest.h>

#include "catalist/tui/tabs/catalogTab.hpp"
#include "support/sampleLibrary.hpp"
#include "tui/tuiHarness.hpp"

using catalist::catalogTab;
using catalist::mediaType;
using catalist::watchStatus;
using ftxui::Event;

namespace {

// Highest score first: Frieren (10), Odd Taxi (8), Ghost (6), Long Title (0).
// Off a terminal the title column is 16 glyphs wide, hence the short names.
const catalist::appSettings& descendingScores(catalist::appSettings& settings) {
  settings.reverseSort = false;
  return settings;
}

struct catalogFixture : catalist::test::tuiHarness {
  catalogFixture() : tab(mediaType::kAnime, descendingScores(settings)) {
    catalist::test::seedSampleLibrary(db);
    tab.onActivate(ctx);
  }
  catalist::entry anime(int id) { return *db.getById(mediaType::kAnime, id); }
  catalogTab tab;
};

}  // namespace

TEST_CASE_FIXTURE(catalogFixture, "tui catalog lists the media sorted by rating") {
  auto shown = screen(tab);
  CHECK(shown.find("Sousou no Frie") != std::string::npos);
  CHECK(shown.find("Berserk") == std::string::npos);
  CHECK(tab.countBadge(ctx) == "4");
}

TEST_CASE_FIXTURE(catalogFixture, "tui catalog plus steps progress and u undoes it") {
  type(tab, "j");
  REQUIRE(press(tab, Event::Character('+')));
  CHECK(anime(44265).progress == 6);
  CHECK(undo.canUndo());
  CHECK(db.getRecentActivity(catalist::mediaType::kAnime, 10).size() == 1);

  REQUIRE(press(tab, Event::Character('u')));
  CHECK(anime(44265).progress == 5);
  CHECK(notices.hasPopup());
}

TEST_CASE_FIXTURE(catalogFixture, "tui catalog equals steps progress like plus") {
  type(tab, "j");
  REQUIRE(press(tab, Event::Character('=')));
  CHECK(anime(44265).progress == 6);
}

TEST_CASE_FIXTURE(catalogFixture, "tui catalog details keys adjust status and rating") {
  REQUIRE(press(tab, Event::Character('l')));
  REQUIRE(press(tab, Event::Character('l')));
  CHECK(anime(52991).status == watchStatus::kOnHold);
  REQUIRE(press(tab, Event::Character('j')));
  REQUIRE(press(tab, Event::Character('h')));
  CHECK(anime(52991).score == 9);
  REQUIRE(press(tab, Event::Escape));
  CHECK(tab.hints().front().key == "Tab");
}

TEST_CASE_FIXTURE(catalogFixture, "tui catalog notes edit is saved, logged and undoable") {
  REQUIRE(press(tab, Event::Character('l')));
  type(tab, "jjj");
  REQUIRE(press(tab, Event::Character('e')));
  REQUIRE(tab.capturesInput());
  type(tab, "ok");
  REQUIRE(press(tab, Event::Return));
  CHECK(anime(52991).notes.find("ok") != std::string::npos);
  CHECK(db.getRecentActivity(mediaType::kAnime, 10).size() == 1);
  CHECK(undo.canUndo());
}

TEST_CASE_FIXTURE(catalogFixture, "tui catalog search filters and captures input") {
  REQUIRE(press(tab, Event::Character('/')));
  CHECK(tab.capturesInput());
  type(tab, "taxi");
  REQUIRE(press(tab, Event::Return));
  CHECK_FALSE(tab.capturesInput());
  auto shown = screen(tab);
  CHECK(shown.find("Odd Taxi") != std::string::npos);
  CHECK(shown.find("Frieren") == std::string::npos);
}

TEST_CASE_FIXTURE(catalogFixture, "tui catalog delete asks for confirmation") {
  REQUIRE(press(tab, Event::Character('d')));
  CHECK(tab.capturesInput());
  REQUIRE(press(tab, Event::Character('n')));
  CHECK(db.getById(mediaType::kAnime, 52991).has_value());

  REQUIRE(press(tab, Event::Character('d')));
  REQUIRE(press(tab, Event::Character('y')));
  CHECK_FALSE(db.getById(mediaType::kAnime, 52991).has_value());
  REQUIRE(press(tab, Event::Character('u')));
  CHECK(db.getById(mediaType::kAnime, 52991).has_value());
}

TEST_CASE_FIXTURE(catalogFixture, "tui catalog visual selection batches status changes") {
  REQUIRE(press(tab, Event::Character(' ')));
  REQUIRE(press(tab, Event::Character('j')));
  REQUIRE(press(tab, Event::Character(' ')));
  CHECK(tab.selectionSummary() == "(2 selected)");
  REQUIRE(press(tab, Event::Character('S')));
  CHECK(anime(52991).status == watchStatus::kOnHold);
  CHECK(notices.status() == "Batch cycled status for 2 entries.");
}

TEST_CASE_FIXTURE(catalogFixture, "tui catalog sort keys toggle direction") {
  REQUIRE(press(tab, Event::Character('t')));
  CHECK(notices.status() == "Sort: Title (Increasing ▲)");
  REQUIRE(press(tab, Event::Character('t')));
  CHECK(notices.status() == "Sort: Title (Decreasing ▼)");
}

TEST_CASE_FIXTURE(catalogFixture, "tui catalog sidebar filters by status") {
  REQUIRE(press(tab, Event::Character('h')));
  type(tab, "j");
  auto shown = screen(tab);
  CHECK(shown.find("Odd Taxi") != std::string::npos);
  CHECK(shown.find("Frieren") == std::string::npos);
}

TEST_CASE_FIXTURE(catalogFixture, "tui catalog open and copy go through the launcher") {
  REQUIRE(press(tab, Event::Character('o')));
  REQUIRE(press(tab, Event::Character('y')));
  REQUIRE(launcher.calls().size() >= 2);
  CHECK(launcher.calls()[0].argv ==
        std::vector<std::string>{"xdg-open", "https://myanimelist.net/anime/52991"});
  CHECK(launcher.calls()[1].standardInput == "Sousou no Frieren");
}

TEST_CASE("tui catalog starts in the default sort from settings") {
  catalist::test::tuiHarness harness;
  catalist::test::seedSampleLibrary(harness.db);
  harness.settings.defaultSort = "title";
  harness.settings.reverseSort = false;
  catalogTab tab(mediaType::kAnime, harness.settings);
  tab.onActivate(harness.ctx);
  auto shown = harness.screen(tab);
  CHECK(shown.find("Title ▲") != std::string::npos);
}
