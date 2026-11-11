#include <doctest/doctest.h>

#include "tui/catalogTab.hpp"
#include "tui/style.hpp"
#include "support/sampleLibrary.hpp"
#include "tui/tuiHarness.hpp"

using catalist::appMode;
using catalist::catalogTab;
using catalist::mediaType;
using catalist::watchStatus;
using ftxui::Event;

namespace {

const char* kFrieren = "Sousou no Frieren";
const char* kOddTaxi = "Odd Taxi";
const char* kGhost = "Ghost, \"in\" the Shell";
const char* kLong = "A Very Long Title That Will Definitely Be Truncated";

// Off a terminal the title column is 16 glyphs wide, so screen checks use
// short names.
struct catalogFixture : catalist::test::tuiHarness {
  catalogFixture() : tab(mediaType::kAnime) {
    catalist::test::seedSampleLibrary(db);
    tab.onActivate(ctx);
  }
  catalist::entry anime(const std::string& title) {
    return catalist::test::findTitle(db, mediaType::kAnime, title);
  }
  std::vector<std::string> rows() const {
    std::vector<std::string> titles;
    for (const auto& e : tab.shown())
      titles.push_back(e.title);
    return titles;
  }
  std::string savedAnime() {
    return fs.readFile("/data/anime.csv").value_or("");
  }
  catalogTab tab;
};

using rowList = std::vector<std::string>;

// A Jikan search reply: Frieren is already tracked, Fullmetal is not.
const char* kSearchReply = R"({"data": [
  {"mal_id": 52991, "title": "Sousou no Frieren", "episodes": 28,
   "score": 9.3, "year": 2023},
  {"mal_id": 5114, "title": "Fullmetal Alchemist: Brotherhood",
   "episodes": 64, "score": 9.1, "year": null,
   "aired": {"from": "2009-04-05T00:00:00+00:00"}}
]})";

}  // namespace

TEST_CASE_FIXTURE(catalogFixture,
                  "tui catalog lists only its media, best rated first") {
  CHECK(rows() == rowList{kFrieren, kOddTaxi, kGhost, kLong});
  CHECK(tab.countBadge(ctx) == "4");
  auto shown = screen(tab);
  CHECK(shown.find("Sousou no Frie") != std::string::npos);
  CHECK(shown.find("Berserk") == std::string::npos);
  CHECK(shown.find(" Rating ▼") != std::string::npos);
}

TEST_CASE_FIXTURE(catalogFixture,
                  "tui catalog new sort column starts in its natural order") {
  REQUIRE(press(tab, Event::Character('t')));
  CHECK(rows() == rowList{kLong, kGhost, kOddTaxi, kFrieren});
  CHECK(ctx.status == "Sort: Title (Increasing ▲)");
  REQUIRE(press(tab, Event::Character('t')));
  CHECK(rows() == rowList{kFrieren, kOddTaxi, kGhost, kLong});
  CHECK(ctx.status == "Sort: Title (Decreasing ▼)");

  // Coming from another column, rating starts high-to-low again.
  REQUIRE(press(tab, Event::Character('r')));
  CHECK(rows() == rowList{kFrieren, kOddTaxi, kGhost, kLong});
  CHECK(ctx.status == "Sort: Rating (Decreasing ▼)");
  REQUIRE(press(tab, Event::Character('r')));
  CHECK(rows() == rowList{kLong, kGhost, kOddTaxi, kFrieren});
  CHECK(ctx.status == "Sort: Rating (Increasing ▲)");
}

TEST_CASE_FIXTURE(catalogFixture,
                  "tui catalog sorts by progress, status and the date shown") {
  REQUIRE(press(tab, Event::Character('p')));
  CHECK(rows() == rowList{kFrieren, kOddTaxi, kGhost, kLong});
  CHECK(ctx.status == "Sort: Progress (Decreasing ▼)");

  REQUIRE(press(tab, Event::Character('s')));
  CHECK(rows() == rowList{kOddTaxi, kFrieren, kGhost, kLong});

  // Long title: last updated 2026-10-18; Frieren: finished 2024-03-22;
  // Odd Taxi: started 2021-04-06; Ghost: started 2020-01-01.
  REQUIRE(press(tab, Event::Character('c')));
  CHECK(rows() == rowList{kLong, kFrieren, kOddTaxi, kGhost});
  CHECK(ctx.status == "Sort: Date (Decreasing ▼)");
}

TEST_CASE_FIXTURE(
    catalogFixture,
    "tui catalog plus and minus step the selected entry and save") {
  type(tab, "j");
  REQUIRE(press(tab, Event::Character('+')));
  CHECK(anime(kOddTaxi).progress == 6);
  CHECK(ctx.status == "Progress: 6/13");
  CHECK(savedAnime().find("Odd Taxi,Watching,8,6,13") != std::string::npos);

  REQUIRE(press(tab, Event::Character('=')));
  REQUIRE(press(tab, Event::Character('P')));
  CHECK(anime(kOddTaxi).progress == 8);
  REQUIRE(press(tab, Event::Character('-')));
  CHECK(anime(kOddTaxi).progress == 7);
  CHECK(anime(kOddTaxi).updatedAt->toString() == "2026-10-20");
}

TEST_CASE_FIXTURE(catalogFixture,
                  "tui catalog S cycles status and R raises the rating") {
  type(tab, "j");
  REQUIRE(press(tab, Event::Character('R')));
  CHECK(anime(kOddTaxi).score == 9);
  CHECK(ctx.status == "Rating: " + catalist::renderStarMeter(9));
  REQUIRE(press(tab, Event::Character('S')));
  CHECK(anime(kOddTaxi).status == watchStatus::kCompleted);
  CHECK(ctx.status == "Status: Completed");
  // The re-sorted list keeps the changed entry selected.
  REQUIRE(press(tab, Event::Character('S')));
  CHECK(anime(kOddTaxi).status == watchStatus::kOnHold);
}

TEST_CASE_FIXTURE(catalogFixture,
                  "tui catalog details keys adjust status and rating") {
  REQUIRE(press(tab, Event::Character('l')));
  REQUIRE(press(tab, Event::Character('l')));
  CHECK(anime(kFrieren).status == watchStatus::kOnHold);
  REQUIRE(press(tab, Event::Character('j')));
  REQUIRE(press(tab, Event::Character('h')));
  CHECK(anime(kFrieren).score == 9);
  REQUIRE(press(tab, Event::Escape));
  CHECK(tab.hints().front().key == "Tab");
}

TEST_CASE_FIXTURE(catalogFixture,
                  "tui catalog plus on a finished title changes nothing") {
  REQUIRE(press(tab, Event::Character('+')));
  auto frieren = anime(kFrieren);
  CHECK(frieren.progress == 28);
  CHECK(catalist::formatDate(frieren.startDate) == "2023-09-29");
  CHECK(catalist::formatDate(frieren.finishDate) == "2024-03-22");
  CHECK(catalist::formatDate(frieren.updatedAt) == "2026-10-17");
}

TEST_CASE_FIXTURE(catalogFixture, "tui catalog details take a typed progress") {
  type(tab, "j");
  REQUIRE(press(tab, Event::Character('l')));
  type(tab, "jj");
  REQUIRE(press(tab, Event::Character('e')));
  CHECK(tab.capturesInput());
  CHECK(tab.mode() == appMode::kEdt);
  CHECK(ctx.status == "Progress: type a number.");
  // The current value is there to edit; letters are ignored.
  REQUIRE(press(tab, Event::Backspace));
  type(tab, "1x0");
  REQUIRE(press(tab, Event::Return));
  CHECK(anime(kOddTaxi).progress == 10);
  CHECK(ctx.status == "Progress: 10/13");
  CHECK(savedAnime().find("Odd Taxi,Watching,8,10,13,") != std::string::npos);
  CHECK_FALSE(tab.capturesInput());

  REQUIRE(press(tab, Event::Return));
  type(tab, "9");
  REQUIRE(press(tab, Event::Escape));
  CHECK(anime(kOddTaxi).progress == 10);
  CHECK(ctx.status == "Progress: left unchanged.");

  REQUIRE(press(tab, Event::Return));
  type(tab, "99");
  REQUIRE(press(tab, Event::Return));
  CHECK(anime(kOddTaxi).progress == 13);
  CHECK(anime(kOddTaxi).status == watchStatus::kCompleted);
}

TEST_CASE_FIXTURE(catalogFixture,
                  "tui catalog details refuse an empty number") {
  REQUIRE(press(tab, Event::Character('l')));
  type(tab, "jj");
  REQUIRE(press(tab, Event::Return));
  REQUIRE(press(tab, Event::Backspace));
  REQUIRE(press(tab, Event::Backspace));
  REQUIRE(press(tab, Event::Return));
  CHECK(ctx.status == "Type a number, or [Esc] to cancel.");
  CHECK(tab.capturesInput());
  // A paste arrives as one event: letters or more than six digits are
  // refused whole, so the number always fits.
  REQUIRE(press(tab, Event::Character("12a")));
  REQUIRE(press(tab, Event::Character("99999999999")));
  REQUIRE(press(tab, Event::Character("123456")));
  REQUIRE(press(tab, Event::Character("7")));
  REQUIRE(press(tab, Event::Return));
  CHECK(anime(kFrieren).progress == 28);
  CHECK(ctx.status == "Progress: 28/28");

  REQUIRE(press(tab, Event::Return));
  REQUIRE(press(tab, Event::Escape));
  CHECK(anime(kFrieren).progress == 28);
}

TEST_CASE_FIXTURE(
    catalogFixture,
    "tui catalog details set the total of a title added by hand") {
  type(tab, "jjj");
  REQUIRE(tab.shown()[3].title == kLong);
  REQUIRE(press(tab, Event::Character('l')));
  type(tab, "jjj");
  REQUIRE(press(tab, Event::Character('e')));
  CHECK(ctx.status == "Episodes: type a number.");
  type(tab, "12");
  REQUIRE(press(tab, Event::Return));
  CHECK(anime(kLong).totalUnits == 12);
  CHECK(ctx.status == "Episodes: 12");
  REQUIRE(press(tab, Event::Character('l')));
  CHECK(anime(kLong).totalUnits == 13);
  CHECK(screen(tab).find("0/13 eps") != std::string::npos);
}

TEST_CASE_FIXTURE(catalogFixture,
                  "tui catalog anime details count rewatches but not volumes") {
  REQUIRE(press(tab, Event::Character('l')));
  type(tab, "jjjjjjj");
  REQUIRE(press(tab, Event::Character('l')));
  CHECK(anime(kFrieren).rewatched == 2);
  CHECK(ctx.status == "Rewatched: 2");
  REQUIRE(press(tab, Event::Character('h')));
  REQUIRE(press(tab, Event::Character('h')));
  REQUIRE(press(tab, Event::Character('h')));
  CHECK(anime(kFrieren).rewatched == 0);
  auto shown = screen(tab);
  CHECK(shown.find("Rewatched:") != std::string::npos);
  CHECK(shown.find("Volumes:") == std::string::npos);
  CHECK(shown.find("NOTES") == std::string::npos);
}

TEST_CASE("tui catalog manga details count volumes and rereads") {
  catalist::test::tuiHarness harness;
  catalist::test::seedSampleLibrary(harness.db);
  catalogTab manga(mediaType::kManga);
  manga.onActivate(harness.ctx);
  auto berserk = [&] {
    return catalist::test::findTitle(harness.db, mediaType::kManga, "Berserk");
  };
  REQUIRE(harness.press(manga, Event::Character('l')));
  harness.type(manga, "jjjj");
  REQUIRE(harness.press(manga, Event::Character('l')));
  CHECK(berserk().volumes == 43);
  CHECK(harness.ctx.status == "Volumes: 43/?");
  REQUIRE(harness.press(manga, Event::Character('j')));
  REQUIRE(harness.press(manga, Event::Character('l')));
  CHECK(berserk().rewatched == 1);
  CHECK(harness.ctx.status == "Reread: 1");
  auto shown = harness.screen(manga);
  CHECK(shown.find("Chapters:") != std::string::npos);
  CHECK(shown.find("43/?") != std::string::npos);
}

TEST_CASE_FIXTURE(catalogFixture,
                  "tui catalog search filters and captures input") {
  REQUIRE(press(tab, Event::Character('/')));
  CHECK(tab.capturesInput());
  CHECK(tab.mode() == appMode::kSch);
  type(tab, "TAXI");
  REQUIRE(press(tab, Event::Return));
  CHECK_FALSE(tab.capturesInput());
  CHECK(rows() == rowList{kOddTaxi});
  CHECK(ctx.status == "Filtered: 'TAXI' (1 matches)");

  REQUIRE(press(tab, Event::Character('/')));
  REQUIRE(press(tab, Event::Escape));
  CHECK(rows().size() == 4);
  CHECK(ctx.status == "Search cleared.");
}

TEST_CASE_FIXTURE(catalogFixture, "tui catalog delete asks first and saves") {
  REQUIRE(press(tab, Event::Character('d')));
  CHECK(tab.capturesInput());
  CHECK(tab.mode() == appMode::kDel);
  // Keys other than y and n do nothing while the question is open.
  REQUIRE(press(tab, Event::Character('q')));
  REQUIRE(press(tab, Event::Character('n')));
  CHECK(db.getEntries(mediaType::kAnime).size() == 4);
  CHECK(ctx.status == "Deletion canceled.");

  REQUIRE(press(tab, Event::Character('d')));
  REQUIRE(press(tab, Event::Character('y')));
  CHECK(rows() == rowList{kOddTaxi, kGhost, kLong});
  CHECK(ctx.status == "Deleted 'Sousou no Frieren'.");
  CHECK(savedAnime().find("Frieren") == std::string::npos);
  CHECK_FALSE(tab.capturesInput());
}

TEST_CASE_FIXTURE(catalogFixture, "tui catalog reports a failed save") {
  fs.failWritesTo("/data/anime.csv");
  REQUIRE(press(tab, Event::Character('+')));
  CHECK(ctx.status == "Could not save the catalogue to /data.");
}

TEST_CASE_FIXTURE(catalogFixture, "tui catalog sidebar filters by status") {
  REQUIRE(press(tab, Event::Character('h')));
  type(tab, "j");
  CHECK(rows() == rowList{kOddTaxi});
  type(tab, "jjj");
  CHECK(rows() == rowList{kGhost});
  // Return goes back to the table and keeps the filter.
  REQUIRE(press(tab, Event::Return));
  CHECK(rows() == rowList{kGhost});
}

TEST_CASE_FIXTURE(
    catalogFixture,
    "tui catalog sidebar has a row for every rating from 10 to 0") {
  const int expectedCounts[11] = {1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1};
  auto shown = screen(tab);
  size_t previousRow = 0;
  for (int score = 10; score >= 0; --score) {
    CAPTURE(score);
    std::string row = (score < 10 ? " " : "") + std::to_string(score) + " " +
                      catalist::renderStarMeter(score) + " (" +
                      std::to_string(expectedCounts[score]) + ")";
    size_t found = shown.find(row);
    REQUIRE(found != std::string::npos);
    CHECK(found > previousRow);
    previousRow = found;
  }
}

TEST_CASE_FIXTURE(catalogFixture, "tui catalog sidebar filters by any rating") {
  REQUIRE(press(tab, Event::Character('h')));
  // All Media, five statuses, then the ratings 10, 9, 8: eight steps down.
  type(tab, "jjjjjjjj");
  CHECK(rows() == rowList{kOddTaxi});
  // The last row is 0, the unrated titles.
  type(tab, "jjjjjjjjjj");
  CHECK(rows() == rowList{kLong});
}

TEST_CASE(
    "tui catalog manga tab says reading and shows an empty library hint") {
  catalist::test::tuiHarness harness;
  catalogTab manga(mediaType::kManga);
  manga.onActivate(harness.ctx);
  auto shown = harness.screen(manga);
  CHECK(shown.find("Reading") != std::string::npos);
  CHECK(shown.find("Nothing here yet. Press a to add a title") !=
        std::string::npos);
}

TEST_CASE_FIXTURE(catalogFixture,
                  "tui catalog add searches MyAnimeList and adds the pick") {
  http.respond("api.jikan.moe/v4/anime?q=fullmetal", kSearchReply);
  REQUIRE(press(tab, Event::Character('a')));
  CHECK(tab.mode() == appMode::kAdd);
  CHECK(tab.capturesInput());
  type(tab, "fullmetal");
  REQUIRE(press(tab, Event::Return));
  CHECK(ctx.status == "Searching MyAnimeList for 'fullmetal'...");
  // The request waits for the next tick, so the status above is drawn first.
  CHECK(http.requested.empty());
  tab.onTick(ctx);
  REQUIRE(http.requested.size() == 1);
  CHECK(ctx.status == "2 matches for 'fullmetal'.");
  auto shown = screen(tab);
  CHECK(shown.find("2009") != std::string::npos);
  CHECK(shown.find("64 eps") != std::string::npos);
  CHECK(shown.find("Add 'fullmetal' by hand") != std::string::npos);

  REQUIRE(press(tab, Event::Character('j')));
  REQUIRE(press(tab, Event::Return));
  auto added = anime("Fullmetal Alchemist: Brotherhood");
  CHECK(added.malId == 5114);
  CHECK(added.totalUnits == 64);
  CHECK(added.progress == 0);
  CHECK(added.status == watchStatus::kPlanToWatch);
  CHECK(added.updatedAt->toString() == "2026-10-20");
  CHECK(ctx.status == "Added 'Fullmetal Alchemist: Brotherhood'.");
  CHECK(tab.shown().size() == 5);
  CHECK_FALSE(tab.capturesInput());
  CHECK(savedAnime().find("Fullmetal Alchemist: Brotherhood") !=
        std::string::npos);
  // The new title is selected, so the next key acts on it.
  REQUIRE(press(tab, Event::Character('+')));
  CHECK(anime("Fullmetal Alchemist: Brotherhood").progress == 1);
}

TEST_CASE_FIXTURE(catalogFixture,
                  "tui catalog add does not duplicate a tracked title") {
  http.respond("api.jikan.moe/v4/anime", kSearchReply);
  REQUIRE(press(tab, Event::Character('a')));
  type(tab, "frieren");
  REQUIRE(press(tab, Event::Return));
  tab.onTick(ctx);
  REQUIRE(press(tab, Event::Return));
  CHECK(db.getEntries(mediaType::kAnime).size() == 4);
  CHECK(ctx.status == "'Sousou no Frieren' is already in the catalogue.");
}

TEST_CASE_FIXTURE(catalogFixture, "tui catalog add works offline by hand") {
  REQUIRE(press(tab, Event::Character('a')));
  type(tab, "Homemade OVA");
  REQUIRE(press(tab, Event::Return));
  tab.onTick(ctx);
  CHECK(ctx.status ==
        "Could not reach MyAnimeList; the last row adds 'Homemade OVA' by "
        "hand.");
  REQUIRE(press(tab, Event::Return));
  auto added = anime("Homemade OVA");
  CHECK(added.malId == 0);
  CHECK(added.status == watchStatus::kPlanToWatch);
  CHECK(db.getEntries(mediaType::kAnime).size() == 5);
}

TEST_CASE_FIXTURE(catalogFixture, "tui catalog add says when nothing matched") {
  http.respond("api.jikan.moe", R"({"data": []})");
  REQUIRE(press(tab, Event::Character('a')));
  type(tab, "zzz");
  REQUIRE(press(tab, Event::Return));
  tab.onTick(ctx);
  CHECK(ctx.status == "No matches for 'zzz'; the last row adds it by hand.");
}

TEST_CASE_FIXTURE(catalogFixture,
                  "tui catalog add can be canceled at any stage") {
  REQUIRE(press(tab, Event::Character('a')));
  REQUIRE(press(tab, Event::Return));
  CHECK(ctx.status == "Type a title first, or [Esc] to cancel.");
  type(tab, "  ");
  REQUIRE(press(tab, Event::Return));
  CHECK(tab.capturesInput());

  REQUIRE(press(tab, Event::Escape));
  CHECK_FALSE(tab.capturesInput());
  CHECK(ctx.status == "Add canceled.");

  REQUIRE(press(tab, Event::Character('a')));
  type(tab, "x");
  REQUIRE(press(tab, Event::Return));
  REQUIRE(press(tab, Event::Escape));
  tab.onTick(ctx);
  CHECK(http.requested.empty());
  CHECK(db.getEntries(mediaType::kAnime).size() == 4);
  CHECK(tab.mode() == appMode::kNor);
}
