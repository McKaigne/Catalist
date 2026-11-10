#include <doctest/doctest.h>

#include "catalist/platform/fileSystem.hpp"
#include "catalist/storage/backup.hpp"
#include "catalist/storage/database.hpp"
#include "support/testSupport.hpp"

using catalist::mediaType;
using catalist::sortKey;
using catalist::watchStatus;
using catalist::test::makeEntry;
using catalist::test::tempDirFixture;

TEST_CASE_FIXTURE(tempDirFixture, "csv round trip keeps quotes commas and newlines") {
  catalist::database db(fileSystem(), clock(), tempDir());
  auto csvPath = tempDir() / "multiline.csv";

  catalist::entry original =
      makeEntry()
          .id(9901)
          .malId(9901)
          .type(mediaType::kAnime)
          .title("Fate/stay night: Heavens Feel, Part \"1\"")
          .status(watchStatus::kCompleted)
          .score(10)
          .progress(3)
          .totalUnits(3)
          .notes("Line 1, with embedded comma.\nLine 2 with \"escaped "
                 "quotes\".\nLine 3.")
          .startDate("2017-10-14")
          .finishDate("2020-08-15");

  db.addEntry(original);
  REQUIRE(db.saveCsv(mediaType::kAnime, csvPath));

  catalist::database reloaded(fileSystem(), clock(), tempDir());
  REQUIRE(reloaded.loadCsv(mediaType::kAnime, csvPath));

  auto loaded = reloaded.getById(mediaType::kAnime, 9901);
  REQUIRE(loaded.has_value());
  CHECK(loaded->title == original.title);
  CHECK(loaded->notes == original.notes);
  CHECK(loaded->score == 10);
  CHECK(loaded->progress == 3);
}

namespace {

void seedSortData(catalist::database& db) {
  db.addEntry(makeEntry().id(52991).malId(52991).anilistId(154587)
                  .title("Sousou no Frieren").status(watchStatus::kCompleted)
                  .score(10).progress(28).totalUnits(28).rewatched(1)
                  .startDate("2023-09-29").finishDate("2024-03-22")
                  .notes("Masterpiece adaptation.").updatedAt("2026-10-17"));
  db.addEntry(makeEntry().id(49387).malId(49387).anilistId(136430)
                  .title("Vinland Saga Season 2").status(watchStatus::kCompleted)
                  .score(9).progress(24).totalUnits(24)
                  .startDate("2023-01-10").finishDate("2023-06-20")
                  .notes("Farmland arc.").updatedAt("2026-10-17"));
  db.addEntry(makeEntry().id(457).malId(457).anilistId(457).title("Mushishi")
                  .status(watchStatus::kCompleted).score(9).progress(26)
                  .totalUnits(26).notes("Atmospheric.")
                  .updatedAt("2026-10-17"));
  db.addEntry(makeEntry().id(44265).malId(44265).anilistId(128547)
                  .title("Odd Taxi").status(watchStatus::kCompleted).score(8)
                  .progress(13).totalUnits(13).startDate("2021-04-06")
                  .finishDate("2021-06-29").notes("Original screenplay.")
                  .updatedAt("2026-10-17"));
  db.addEntry(makeEntry().id(2).malId(2).anilistId(30002).title("Berserk")
                  .type(mediaType::kManga).status(watchStatus::kWatching)
                  .score(10).progress(364).volumes(42)
                  .notes("Dark fantasy pinnacle.").updatedAt("2026-10-17"));
}

}  // namespace

TEST_CASE_FIXTURE(tempDirFixture, "sortEntries sorts by score descending") {
  catalist::database db(fileSystem(), clock(), tempDir());
  seedSortData(db);

  auto byScore = db.getEntries(mediaType::kAnime);
  catalist::database::sortEntries(byScore, sortKey::kScore, false);
  REQUIRE(byScore.size() == 4);
  CHECK(byScore.front().title == "Sousou no Frieren");
  CHECK(byScore.back().title == "Odd Taxi");
}

namespace {

void loadFixture(catalist::database& db, mediaType type,
                 const std::string& name) {
  REQUIRE(db.loadCsv(type, catalist::test::fixturePath("csv/" + name)));
}

}  // namespace

TEST_CASE_FIXTURE(tempDirFixture, "csv loads legacy seven column layout") {
  catalist::database db(fileSystem(), clock(), tempDir());
  loadFixture(db, mediaType::kAnime, "legacy7.csv");
  const auto& entries = db.getEntries(mediaType::kAnime);
  REQUIRE(entries.size() == 2);

  CHECK(entries[0].id == 52991);
  CHECK(entries[0].malId == 52991);
  CHECK(entries[0].title == "Sousou no Frieren");
  CHECK(entries[0].status == watchStatus::kCompleted);
  CHECK(entries[0].score == 10);
  CHECK(entries[0].progress == 28);
  CHECK(entries[0].totalUnits == 28);
  CHECK(entries[0].anilistId == 0);
  CHECK(entries[0].getRemoteId("mal") == "52991");

  CHECK(entries[1].title == "86");
  CHECK(entries[1].anilistId == 0);
}

TEST_CASE_FIXTURE(tempDirFixture, "csv loads offset three layout without type column") {
  catalist::database db(fileSystem(), clock(), tempDir());
  loadFixture(db, mediaType::kAnime, "offset3.csv");
  const auto& entries = db.getEntries(mediaType::kAnime);
  REQUIRE(entries.size() == 1);
  const auto& loaded = entries[0];
  CHECK(loaded.anilistId == 154587);
  CHECK(loaded.title == "Sousou no Frieren");
  CHECK(loaded.status == watchStatus::kCompleted);
  CHECK(loaded.rewatched == 1);
  CHECK(catalist::formatDate(loaded.startDate) == "2023-09-29");
  CHECK(catalist::formatDate(loaded.finishDate) == "2024-03-22");
  CHECK(loaded.notes == "Masterpiece.");
  CHECK(catalist::formatDate(loaded.updatedAt) == "2026-10-17");
  CHECK(loaded.dirty);
}

TEST_CASE_FIXTURE(tempDirFixture, "csv loads current layout with external ids") {
  catalist::database db(fileSystem(), clock(), tempDir());
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
  CHECK_FALSE(loaded.dirty);
  CHECK(loaded.getRemoteId("kitsu") == "1234");
  CHECK(loaded.getRemoteId("mal") == "2");
  CHECK(loaded.getRemoteId("anilist") == "30002");
}

TEST_CASE_FIXTURE(tempDirFixture, "csv loads crlf line endings") {
  catalist::database db(fileSystem(), clock(), tempDir());
  loadFixture(db, mediaType::kAnime, "crlf.csv");
  const auto& entries = db.getEntries(mediaType::kAnime);
  REQUIRE(entries.size() == 1);
  CHECK(entries[0].title == "Odd Taxi");
  CHECK(entries[0].notes == "Crlf notes");
  CHECK(catalist::formatDate(entries[0].updatedAt) == "2026-10-17");
  CHECK(entries[0].remoteIds.size() == 1);
}

TEST_CASE_FIXTURE(tempDirFixture, "csv loads quoted multiline notes") {
  catalist::database db(fileSystem(), clock(), tempDir());
  loadFixture(db, mediaType::kAnime, "multiline.csv");
  const auto& entries = db.getEntries(mediaType::kAnime);
  REQUIRE(entries.size() == 2);
  CHECK(entries[0].title == "Ghost, \"in\" the Shell");
  CHECK(entries[0].notes ==
        "Line one\nLine \"two\", with comma\n\nafter blank");
  CHECK(entries[1].title == "Next");
}

TEST_CASE_FIXTURE(tempDirFixture, "addEntry re-add keeps user fields and refreshes upstream metadata") {
  catalist::database db(fileSystem(), clock(), tempDir());
  db.addEntry(makeEntry().id(52991).malId(52991).title("Sousou no Frieren")
                  .score(8).progress(5).totalUnits(0).notes("mine")
                  .startDate("2023-09-29"));

  int mergedId = db.addEntry(makeEntry().id(52991).malId(52991)
                                 .title("Frieren (remote)").score(3)
                                 .progress(20).totalUnits(28).totalVolumes(4)
                                 .notes("remote notes").startDate("1999-01-01")
                                 .remoteId("kitsu", "46474"));

  CHECK(mergedId == 52991);
  REQUIRE(db.getEntries(mediaType::kAnime).size() == 1);
  auto merged = db.getById(mediaType::kAnime, 52991);
  REQUIRE(merged.has_value());
  CHECK(merged->score == 8);
  CHECK(merged->progress == 5);
  CHECK(merged->notes == "mine");
  CHECK(catalist::formatDate(merged->startDate) == "2023-09-29");
  CHECK(merged->title == "Sousou no Frieren");
  CHECK(merged->totalUnits == 28);
  CHECK(merged->totalVolumes == 4);
  CHECK(merged->getRemoteId("kitsu") == "46474");
}

TEST_CASE_FIXTURE(tempDirFixture, "addEntry re-add backfills empty user fields") {
  catalist::database db(fileSystem(), clock(), tempDir());
  db.addEntry(makeEntry().id(100).malId(100).title("Blank"));
  db.addEntry(makeEntry().id(100).malId(100).title("Blank").score(7).progress(3)
                  .notes("filled").finishDate("2024-01-01").anilistId(55));

  auto merged = db.getById(mediaType::kAnime, 100);
  REQUIRE(merged.has_value());
  CHECK(merged->score == 7);
  CHECK(merged->progress == 3);
  CHECK(merged->notes == "filled");
  CHECK(catalist::formatDate(merged->finishDate) == "2024-01-01");
  CHECK(merged->anilistId == 55);
}

TEST_CASE_FIXTURE(tempDirFixture, "addEntry matches an existing entry by mal remote id") {
  catalist::database db(fileSystem(), clock(), tempDir());
  db.addEntry(makeEntry().id(10001).title("Local").remoteId("mal", "777"));
  int mergedId = db.addEntry(makeEntry().id(0).title("Remote").totalUnits(12)
                                 .remoteId("mal", "777"));
  CHECK(mergedId == 10001);
  REQUIRE(db.getEntries(mediaType::kAnime).size() == 1);
  CHECK(db.getEntries(mediaType::kAnime)[0].totalUnits == 12);
}

TEST_CASE("addEntry never merges two titles that only share a local id") {
  catalist::memoryFileSystem fs;
  auto clock = catalist::test::makeTestClock();
  catalist::database db(fs, clock, "/data");
  db.addEntry(makeEntry().id(0).malId(52991).title("First"));
  int titleOnly = db.addEntry(makeEntry().id(0).title("Title only"));
  db.addEntry(makeEntry().id(titleOnly).malId(titleOnly).title("From MAL"));

  CHECK(db.getEntries(mediaType::kAnime).size() == 3);
  auto kept = db.getById(mediaType::kAnime, titleOnly);
  REQUIRE(kept.has_value());
  CHECK(kept->title == "Title only");
  CHECK(kept->malId == 0);
}

TEST_CASE("database saves and reloads through an in-memory file system") {
  catalist::memoryFileSystem fs;
  auto clock = catalist::test::makeTestClock();
  catalist::database db(fs, clock, "/data");
  db.addEntry(makeEntry().id(7).title("Kept").notes("a, \"b\"\nc"));
  db.logActivity({"Oct 20, 12:00 PM", mediaType::kAnime, "Kept",
                  watchStatus::kWatching, 1, 2, 3});
  REQUIRE(db.saveAll());
  CHECK(fs.exists("/data/anime.csv"));
  CHECK(fs.exists("/data/manga.csv"));
  CHECK(fs.exists("/data/history.csv"));

  catalist::database reloaded(fs, clock, "/data");
  REQUIRE(reloaded.loadAll());
  auto kept = reloaded.getById(mediaType::kAnime, 7);
  REQUIRE(kept.has_value());
  CHECK(kept->notes == "a, \"b\"\nc");
  REQUIRE(reloaded.getRecentActivity(mediaType::kAnime).size() == 1);
  CHECK(reloaded.getRecentActivity(mediaType::kAnime)[0].score == 3);
}

TEST_CASE("database reports a failed atomic write") {
  catalist::memoryFileSystem fs;
  auto clock = catalist::test::makeTestClock();
  catalist::database db(fs, clock, "/data");
  fs.failWritesTo("/data/anime.csv");
  CHECK_FALSE(db.saveCsv(mediaType::kAnime, "/data/anime.csv"));
  CHECK_FALSE(fs.exists("/data/anime.csv"));
}

TEST_CASE("database backup snapshot copies the catalogue with a clock stamp") {
  catalist::memoryFileSystem fs;
  auto clock = catalist::test::makeTestClock();
  catalist::database db(fs, clock, "/data");
  db.addEntry(makeEntry().id(1).title("Snap"));
  REQUIRE(db.saveAll());

  db.createBackupSnapshot();
  auto copy = fs.readFile("/data/backups/anime_20261020_120000.csv");
  REQUIRE(copy.has_value());
  CHECK(*copy == fs.readFile("/data/anime.csv"));
  CHECK(fs.exists("/data/backups/manga_20261020_120000.csv"));
}

TEST_CASE("backup snapshot read and written back restores its catalogue file") {
  catalist::memoryFileSystem fs;
  auto clock = catalist::test::makeTestClock();
  catalist::database db(fs, clock, "/data");
  db.addEntry(makeEntry().id(1).title("Original"));
  REQUIRE(db.saveAll());
  db.createBackupSnapshot();
  db.addEntry(makeEntry().id(2).title("Added later"));
  REQUIRE(db.saveAll());

  auto snapshot = catalist::backup::readSnapshot(fs, "/data",
                                                 "anime_20261020_120000.csv");
  REQUIRE(snapshot.has_value());
  REQUIRE(catalist::backup::writeSnapshot(fs, "/data", *snapshot));
  REQUIRE(db.loadAll());
  CHECK(db.getEntries(mediaType::kAnime).size() == 1);
  CHECK_FALSE(catalist::backup::readSnapshot(fs, "/data", "../anime.csv"));
  CHECK_FALSE(catalist::backup::readSnapshot(fs, "/data", "notes_1.csv"));
  CHECK_FALSE(catalist::backup::readSnapshot(fs, "/data", "anime_missing.csv"));
}

TEST_CASE("backup exportCatalog writes both catalogues to exports") {
  catalist::memoryFileSystem fs;
  auto clock = catalist::test::makeTestClock();
  catalist::database db(fs, clock, "/data");
  db.addEntry(makeEntry().id(1).title("Exported"));
  auto written = catalist::backup::exportCatalog(fs, clock, db);
  REQUIRE(written.size() == 2);
  auto anime = fs.readFile("/data/exports/anime_20261020_120000.csv");
  REQUIRE(anime.has_value());
  CHECK(anime->find("Exported") != std::string::npos);
}

TEST_CASE_FIXTURE(tempDirFixture, "database remembers deleting a synced entry until each service settles") {
  catalist::database db(fileSystem(), clock(), tempDir());
  catalist::entry synced = catalist::test::makeEntry().id(7).malId(7).title("Seven");
  synced.syncedWith = {"mal", "anilist"};
  db.addEntry(synced);
  db.addEntry(catalist::test::makeEntry().id(8).title("Local only"));
  REQUIRE(db.removeEntry(mediaType::kAnime, 7));
  REQUIRE(db.removeEntry(mediaType::kAnime, 8));
  REQUIRE(db.pendingDeletions().size() == 1);
  REQUIRE(db.saveAll());

  catalist::database reloaded(fileSystem(), clock(), tempDir());
  reloaded.loadAll();
  auto pending = reloaded.pendingDeletions();
  REQUIRE(pending.size() == 1);
  reloaded.settleDeletion(pending[0], "mal");
  CHECK(reloaded.pendingDeletions()[0].pendingServices ==
        std::set<std::string>{"anilist"});
  reloaded.settleDeletion(pending[0], "anilist");
  CHECK(reloaded.pendingDeletions().empty());
}

TEST_CASE_FIXTURE(tempDirFixture, "database re-adding a deleted entry cancels its remote deletion") {
  catalist::database db(fileSystem(), clock(), tempDir());
  catalist::entry synced = catalist::test::makeEntry().id(7).malId(7).title("Seven");
  synced.syncedWith = {"mal"};
  db.addEntry(synced);
  REQUIRE(db.removeEntry(mediaType::kAnime, 7));
  db.addEntry(synced);
  CHECK(db.pendingDeletions().empty());
}
