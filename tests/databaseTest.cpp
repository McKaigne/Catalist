#include <doctest/doctest.h>

#include <algorithm>
#include <cstdlib>

#include "catalist/storage/database.hpp"
#include "support/sampleLibrary.hpp"
#include "support/testSupport.hpp"

using catalist::database;
using catalist::mediaType;
using catalist::sortKey;
using catalist::watchStatus;
using catalist::test::findTitle;
using catalist::test::makeEntry;
using catalist::test::memoryFileSystem;
using catalist::test::tempDirFixture;

namespace {

std::vector<std::string> titlesSortedBy(const database& db, sortKey key,
                                        bool reverse) {
  auto entries = db.getEntries(mediaType::kAnime);
  database::sortEntries(entries, key, reverse);
  std::vector<std::string> titles;
  for (const auto& e : entries)
    titles.push_back(e.title);
  return titles;
}

using titleList = std::vector<std::string>;

void loadFixture(database& db, mediaType type, const std::string& name) {
  REQUIRE(db.loadCsv(type, catalist::test::fixturePath("csv/" + name)).value());
}

}  // namespace

TEST_CASE_FIXTURE(tempDirFixture,
                  "csv round trip keeps quotes commas and newlines") {
  database db(fileSystem(), tempDir());
  catalist::entry original =
      makeEntry()
          .malId(9901)
          .title("Fate/stay night: Heavens Feel, Part \"1\"")
          .status(watchStatus::kCompleted)
          .score(10)
          .progress(3)
          .totalUnits(3)
          .notes(
              "Line 1, with embedded comma.\nLine 2 with \"escaped "
              "quotes\".\nLine 3.")
          .startDate("2017-10-14")
          .finishDate("2020-08-15");
  int id = db.addEntry(original);
  REQUIRE(db.saveAll());

  database reloaded(fileSystem(), tempDir());
  REQUIRE(reloaded.loadAll().value());
  auto loaded = reloaded.getById(mediaType::kAnime, id);
  REQUIRE(loaded.has_value());
  CHECK(loaded->title == original.title);
  CHECK(loaded->notes == original.notes);
  CHECK(loaded->score == 10);
  CHECK(loaded->progress == 3);
  CHECK(loaded->startDate == original.startDate);
  CHECK(loaded->finishDate == original.finishDate);
  CHECK_FALSE(loaded->updatedAt.has_value());
}

TEST_CASE("database gives each new title the next free id per media type") {
  memoryFileSystem fs;
  database db(fs, "/data");
  CHECK(db.addEntry(makeEntry().title("One")) == 1);
  CHECK(db.addEntry(makeEntry().title("Two").malId(5)) == 2);
  // An id the caller sets is not kept: ids are only row numbers.
  CHECK(db.addEntry(makeEntry().id(500).title("Three")) == 3);
  CHECK(db.addEntry(makeEntry().title("Manga").type(mediaType::kManga)) == 1);

  REQUIRE(db.removeEntry(mediaType::kAnime, 2));
  CHECK(db.addEntry(makeEntry().title("Four")) == 4);
  REQUIRE(db.removeEntry(mediaType::kAnime, 4));
  REQUIRE(db.removeEntry(mediaType::kAnime, 3));
  CHECK(db.addEntry(makeEntry().title("Five")) == 2);
  CHECK_FALSE(db.removeEntry(mediaType::kAnime, 99));
}

TEST_CASE("database continues numbering after the ids in the loaded file") {
  memoryFileSystem fs;
  REQUIRE(fs.writeFileAtomic("/data/anime.csv",
                             "40,0,0,anime,Loaded,Watching,0,0,0,0,0,0,,,,\n"));
  database db(fs, "/data");
  REQUIRE(db.loadAll().value());
  CHECK(db.getById(mediaType::kAnime, 40)->title == "Loaded");
  CHECK(db.addEntry(makeEntry().title("Next")) == 41);
}

TEST_CASE("database getById and findByMalId look in one media type only") {
  memoryFileSystem fs;
  database db(fs, "/data");
  int anime = db.addEntry(makeEntry().title("Anime").malId(1));
  db.addEntry(makeEntry().title("Manga").malId(1).type(mediaType::kManga));
  CHECK(db.getById(mediaType::kAnime, anime)->title == "Anime");
  CHECK(db.findByMalId(mediaType::kManga, 1)->title == "Manga");
  CHECK_FALSE(db.findByMalId(mediaType::kAnime, 2).has_value());
  CHECK_FALSE(db.findByMalId(mediaType::kAnime, 0).has_value());
  CHECK_FALSE(db.getById(mediaType::kAnime, 99).has_value());
}

TEST_CASE("database updateEntry replaces the entry with the same id") {
  memoryFileSystem fs;
  database db(fs, "/data");
  int id = db.addEntry(makeEntry().title("Before"));
  auto changed = *db.getById(mediaType::kAnime, id);
  changed.title = "After";
  CHECK(db.updateEntry(changed));
  CHECK(db.getById(mediaType::kAnime, id)->title == "After");
  changed.id = 99;
  CHECK_FALSE(db.updateEntry(changed));
}

TEST_CASE(
    "sortEntries puts each key in its natural order and reverse flips it") {
  memoryFileSystem fs;
  database db(fs, "/data");
  catalist::test::seedSampleLibrary(db);
  const titleList byRating = {"Sousou no Frieren", "Odd Taxi",
                              "Ghost, \"in\" the Shell",
                              "A Very Long Title That Will Definitely Be "
                              "Truncated"};
  CHECK(titlesSortedBy(db, sortKey::kScore, false) == byRating);
  CHECK(titlesSortedBy(db, sortKey::kScore, true) ==
        titleList(byRating.rbegin(), byRating.rend()));
  CHECK(titlesSortedBy(db, sortKey::kProgress, false) == byRating);

  CHECK(titlesSortedBy(db, sortKey::kTitle, false) ==
        titleList{"A Very Long Title That Will Definitely Be Truncated",
                  "Ghost, \"in\" the Shell", "Odd Taxi", "Sousou no Frieren"});
  CHECK(titlesSortedBy(db, sortKey::kStatus, false) ==
        titleList{"Odd Taxi", "Sousou no Frieren", "Ghost, \"in\" the Shell",
                  "A Very Long Title That Will Definitely Be Truncated"});
  // By the date the list shows: finish when completed, else start, else the
  // last change.
  CHECK(titlesSortedBy(db, sortKey::kDate, false) ==
        titleList{"A Very Long Title That Will Definitely Be Truncated",
                  "Sousou no Frieren", "Odd Taxi", "Ghost, \"in\" the Shell"});
}

TEST_CASE("sortEntries breaks ties by title") {
  memoryFileSystem fs;
  database db(fs, "/data");
  db.addEntry(makeEntry().title("Charlie").score(7));
  db.addEntry(makeEntry().title("Alpha").score(7));
  db.addEntry(makeEntry().title("Bravo").score(9));
  CHECK(titlesSortedBy(db, sortKey::kScore, false) ==
        titleList{"Bravo", "Alpha", "Charlie"});
}

TEST_CASE("database statistics count, average and estimate time") {
  memoryFileSystem fs;
  database db(fs, "/data");
  catalist::test::seedSampleLibrary(db);
  CHECK(db.countByStatus(mediaType::kAnime, watchStatus::kWatching) == 1);
  CHECK(db.countByStatus(mediaType::kAnime, watchStatus::kOnHold) == 0);
  CHECK(db.countByStatus(mediaType::kManga, watchStatus::kOnHold) == 1);
  CHECK(db.countByScore(mediaType::kAnime, 10) == 1);
  CHECK(db.countByScore(mediaType::kAnime, 0) == 1);
  // Unrated titles do not pull the mean down.
  CHECK(db.averageScore(mediaType::kAnime) == doctest::Approx(8.0));
  CHECK(db.averageScore(mediaType::kManga) == doctest::Approx(9.5));
  CHECK(db.totalProgress(mediaType::kAnime) == 36);
  CHECK(db.totalProgress(mediaType::kManga) == 461);
  CHECK(db.totalVolumes(mediaType::kManga) == 53);
  CHECK(db.totalRewatched(mediaType::kAnime) == 1);
  CHECK(db.daysSpent(mediaType::kAnime) == doctest::Approx(36 * 24 / 1440.0));
  CHECK(db.daysSpent(mediaType::kManga) == doctest::Approx(461 * 5 / 1440.0));

  database empty(fs, "/elsewhere");
  CHECK(empty.averageScore(mediaType::kAnime) == 0.0);
  CHECK(empty.daysSpent(mediaType::kAnime) == 0.0);
}

TEST_CASE("database refuses a catalogue in an old layout and names the file") {
  memoryFileSystem fs;
  for (const char* oldRow :
       {"52991,52991,Sousou no Frieren,Completed,10,28,28\n",
        "52991,52991,154587,Sousou no Frieren,Completed,10,28,28,0,0,1,"
        "2023-09-29,2024-03-22,Masterpiece.,2026-10-17,1\n"}) {
    REQUIRE(fs.writeFileAtomic("/data/anime.csv", oldRow));
    database db(fs, "/data");
    auto loaded = db.loadAll();
    REQUIRE_FALSE(loaded.ok());
    CHECK(loaded.errorMessage().find(
              "/data/anime.csv: row 1 has a column layout") == 0);
    CHECK(db.getEntries(mediaType::kAnime).empty());
  }
}

TEST_CASE("database loadAll says whether there was a catalogue") {
  memoryFileSystem fs;
  database db(fs, "/data");
  auto loaded = db.loadAll();
  REQUIRE(loaded.ok());
  CHECK_FALSE(loaded.value());
  REQUIRE(fs.writeFileAtomic("/data/manga.csv", ""));
  CHECK(db.loadAll().value());
}

TEST_CASE_FIXTURE(tempDirFixture,
                  "csv loads current layout with external ids") {
  database db(fileSystem(), tempDir());
  loadFixture(db, mediaType::kManga, "current.csv");
  const auto& entries = db.getEntries(mediaType::kManga);
  REQUIRE(entries.size() == 1);
  const auto& loaded = entries[0];
  CHECK(loaded.type == mediaType::kManga);
  CHECK(loaded.title == "Berserk");
  CHECK(loaded.status == watchStatus::kWatching);
  CHECK(loaded.progress == 364);
  CHECK(loaded.volumes == 42);
  CHECK(catalist::formatDate(loaded.finishDate) == "");
  CHECK(loaded.malId == 2);
  CHECK(loaded.anilistId == 30002);
}

TEST_CASE_FIXTURE(tempDirFixture, "csv loads crlf line endings") {
  database db(fileSystem(), tempDir());
  loadFixture(db, mediaType::kAnime, "crlf.csv");
  const auto& entries = db.getEntries(mediaType::kAnime);
  REQUIRE(entries.size() == 1);
  CHECK(entries[0].title == "Odd Taxi");
  CHECK(entries[0].notes == "Crlf notes");
  CHECK(catalist::formatDate(entries[0].updatedAt) == "2026-10-17");
  CHECK(entries[0].malId == 44265);
}

TEST_CASE_FIXTURE(tempDirFixture, "csv loads quoted multiline notes") {
  database db(fileSystem(), tempDir());
  loadFixture(db, mediaType::kAnime, "multiline.csv");
  const auto& entries = db.getEntries(mediaType::kAnime);
  REQUIRE(entries.size() == 2);
  CHECK(entries[0].title == "Ghost, \"in\" the Shell");
  CHECK(entries[0].notes ==
        "Line one\nLine \"two\", with comma\n\nafter blank");
  CHECK(entries[1].title == "Next");
}

TEST_CASE("addEntry re-add keeps user fields and refreshes upstream metadata") {
  memoryFileSystem fs;
  database db(fs, "/data");
  int id = db.addEntry(makeEntry()
                           .malId(52991)
                           .title("Sousou no Frieren")
                           .score(8)
                           .progress(5)
                           .totalUnits(0)
                           .notes("mine")
                           .startDate("2023-09-29"));
  int mergedId = db.addEntry(makeEntry()
                                 .malId(52991)
                                 .title("Frieren (remote)")
                                 .score(3)
                                 .progress(20)
                                 .totalUnits(28)
                                 .totalVolumes(4)
                                 .notes("remote notes")
                                 .startDate("1999-01-01"));

  CHECK(mergedId == id);
  REQUIRE(db.getEntries(mediaType::kAnime).size() == 1);
  auto merged = *db.getById(mediaType::kAnime, id);
  CHECK(merged.score == 8);
  CHECK(merged.progress == 5);
  CHECK(merged.notes == "mine");
  CHECK(catalist::formatDate(merged.startDate) == "2023-09-29");
  CHECK(merged.title == "Sousou no Frieren");
  CHECK(merged.totalUnits == 28);
  CHECK(merged.totalVolumes == 4);
}

TEST_CASE("addEntry re-add backfills empty user fields") {
  memoryFileSystem fs;
  database db(fs, "/data");
  int id = db.addEntry(makeEntry().malId(100).title("Blank"));
  db.addEntry(makeEntry()
                  .malId(100)
                  .title("Blank")
                  .score(7)
                  .progress(3)
                  .notes("filled")
                  .finishDate("2024-01-01")
                  .anilistId(55));
  auto merged = *db.getById(mediaType::kAnime, id);
  CHECK(merged.score == 7);
  CHECK(merged.progress == 3);
  CHECK(merged.notes == "filled");
  CHECK(catalist::formatDate(merged.finishDate) == "2024-01-01");
  CHECK(merged.anilistId == 55);
}

TEST_CASE("addEntry matches by MyAnimeList or AniList id, never by title") {
  memoryFileSystem fs;
  database db(fs, "/data");
  db.addEntry(makeEntry().malId(777).title("Local"));
  db.addEntry(makeEntry().title("Remote").totalUnits(12).malId(777));
  db.addEntry(makeEntry().anilistId(88).title("On AniList"));
  db.addEntry(makeEntry().anilistId(88).title("On AniList").totalUnits(3));
  db.addEntry(makeEntry().title("Same title"));
  db.addEntry(makeEntry().title("Same title"));

  const auto& anime = db.getEntries(mediaType::kAnime);
  REQUIRE(anime.size() == 4);
  CHECK(findTitle(db, mediaType::kAnime, "Local").totalUnits == 12);
  CHECK(findTitle(db, mediaType::kAnime, "On AniList").totalUnits == 3);
  CHECK(std::count_if(anime.begin(), anime.end(), [](const auto& e) {
          return e.title == "Same title";
        }) == 2);
}

TEST_CASE("database saves both lists and reloads them") {
  memoryFileSystem fs;
  database db(fs, "/data");
  int id = db.addEntry(makeEntry().title("Kept").notes("a, \"b\"\nc"));
  db.addEntry(makeEntry().title("Paper").type(mediaType::kManga));
  REQUIRE(db.saveAll());
  CHECK(fs.fileCount() == 2);

  database reloaded(fs, "/data");
  REQUIRE(reloaded.loadAll().value());
  CHECK(reloaded.getById(mediaType::kAnime, id)->notes == "a, \"b\"\nc");
  CHECK(reloaded.getEntries(mediaType::kManga)[0].title == "Paper");
  CHECK(reloaded.getEntries(mediaType::kManga)[0].type == mediaType::kManga);
}

TEST_CASE("database reports a failed write") {
  memoryFileSystem fs;
  database db(fs, "/data");
  fs.failWritesTo("/data/manga.csv");
  CHECK_FALSE(db.saveAll());
  CHECK_FALSE(fs.readFile("/data/manga.csv").has_value());
}

TEST_CASE("database data dir comes from CATALIST_DATA_DIR first") {
  // The test main points CATALIST_DATA_DIR at a sandbox.
  CHECK(database::getDefaultConfigDir() == std::getenv("CATALIST_DATA_DIR"));
  memoryFileSystem fs;
  database db(fs, "/data");
  CHECK(db.getConfigPath() == "/data");
  CHECK(db.getCsvPath(mediaType::kAnime) == "/data/anime.csv");
  CHECK(db.getCsvPath(mediaType::kManga) == "/data/manga.csv");
}
