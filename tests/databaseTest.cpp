#include <doctest/doctest.h>

#include "catalist/database.hpp"
#include "support/testSupport.hpp"

using catalist::MediaType;
using catalist::SortKey;
using catalist::WatchStatus;
using catalist::test::makeEntry;
using catalist::test::tempDirFixture;

TEST_CASE_FIXTURE(tempDirFixture, "csv round trip keeps quotes commas and newlines") {
  catalist::Database db(tempDir());
  auto csvPath = tempDir() / "multiline.csv";

  catalist::Entry original =
      makeEntry()
          .id(9901)
          .malId(9901)
          .type(MediaType::Anime)
          .title("Fate/stay night: Heavens Feel, Part \"1\"")
          .status(WatchStatus::Completed)
          .score(10)
          .progress(3)
          .totalUnits(3)
          .notes("Line 1, with embedded comma.\nLine 2 with \"escaped "
                 "quotes\".\nLine 3.")
          .startDate("2017-10-14")
          .finishDate("2020-08-15");

  db.addEntry(original);
  REQUIRE(db.saveCsv(MediaType::Anime, csvPath));

  catalist::Database reloaded(tempDir());
  REQUIRE(reloaded.loadCsv(MediaType::Anime, csvPath));

  auto loaded = reloaded.getById(MediaType::Anime, 9901);
  REQUIRE(loaded.has_value());
  CHECK(loaded->title == original.title);
  CHECK(loaded->notes == original.notes);
  CHECK(loaded->score == 10);
  CHECK(loaded->progress == 3);
}

namespace {

void seedSortData(catalist::Database& db) {
  db.addEntry(makeEntry().id(52991).malId(52991).anilistId(154587)
                  .title("Sousou no Frieren").status(WatchStatus::Completed)
                  .score(10).progress(28).totalUnits(28).rewatched(1)
                  .startDate("2023-09-29").finishDate("2024-03-22")
                  .notes("Masterpiece adaptation.").updatedAt("2026-10-17"));
  db.addEntry(makeEntry().id(49387).malId(49387).anilistId(136430)
                  .title("Vinland Saga Season 2").status(WatchStatus::Completed)
                  .score(9).progress(24).totalUnits(24)
                  .startDate("2023-01-10").finishDate("2023-06-20")
                  .notes("Farmland arc.").updatedAt("2026-10-17"));
  db.addEntry(makeEntry().id(457).malId(457).anilistId(457).title("Mushishi")
                  .status(WatchStatus::Completed).score(9).progress(26)
                  .totalUnits(26).startDate("0000-00-00")
                  .finishDate("0000-00-00").notes("Atmospheric.")
                  .updatedAt("2026-10-17"));
  db.addEntry(makeEntry().id(44265).malId(44265).anilistId(128547)
                  .title("Odd Taxi").status(WatchStatus::Completed).score(8)
                  .progress(13).totalUnits(13).startDate("2021-04-06")
                  .finishDate("2021-06-29").notes("Original screenplay.")
                  .updatedAt("2026-10-17"));
  db.addEntry(makeEntry().id(2).malId(2).anilistId(30002).title("Berserk")
                  .type(MediaType::Manga).status(WatchStatus::Watching)
                  .score(10).progress(364).volumes(42)
                  .startDate("0000-00-00").finishDate("0000-00-00")
                  .notes("Dark fantasy pinnacle.").updatedAt("2026-10-17"));
}

}  // namespace

TEST_CASE_FIXTURE(tempDirFixture, "filterEntries sorts by score descending") {
  catalist::Database db(tempDir());
  seedSortData(db);

  auto byScore = db.filterEntries(MediaType::Anime, std::nullopt, -1, "",
                                  SortKey::Score, false);
  REQUIRE(byScore.size() == 4);
  CHECK(byScore.front().title == "Sousou no Frieren");
  CHECK(byScore.back().title == "Odd Taxi");
}

TEST_CASE_FIXTURE(tempDirFixture, "filterEntries matches title substring case insensitively") {
  catalist::Database db(tempDir());
  seedSortData(db);

  auto searched = db.filterEntries(MediaType::Anime, std::nullopt, -1,
                                   "frieren", SortKey::Title, false);
  REQUIRE(searched.size() == 1);
  CHECK(searched[0].id == 52991);
}
