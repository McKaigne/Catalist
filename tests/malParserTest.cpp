#include <doctest/doctest.h>

#include "catalist/storage/database.hpp"
#include "catalist/malParser.hpp"
#include "support/testSupport.hpp"

using catalist::test::tempDirFixture;

TEST_CASE_FIXTURE(tempDirFixture, "malParser imports an anime block with CDATA") {
  auto xmlPath = tempDir() / "mal.xml";
  catalist::test::writeTextFile(
      xmlPath,
      "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"
      "<myanimelist>\n"
      "  <anime>\n"
      "    <series_animedb_id>52991</series_animedb_id>\n"
      "    <series_title><![CDATA[Sousou no Frieren]]></series_title>\n"
      "    <series_episodes>28</series_episodes>\n"
      "    <my_watched_episodes>28</my_watched_episodes>\n"
      "    <my_score>10</my_score>\n"
      "    <my_status>Completed</my_status>\n"
      "    <my_start_date>2023-09-29</my_start_date>\n"
      "    <my_finish_date>2024-03-22</my_finish_date>\n"
      "    <my_comments><![CDATA[Masterpiece.]]></my_comments>\n"
      "  </anime>\n"
      "</myanimelist>\n");

  catalist::database db(fileSystem(), clock(), tempDir());
  CHECK(catalist::malParser::importXml(fileSystem(), xmlPath.string(), db, true) == 1);

  auto imported = db.getById(catalist::mediaType::kAnime, 52991);
  REQUIRE(imported.has_value());
  CHECK(imported->title == "Sousou no Frieren");
  CHECK(imported->score == 10);
  CHECK(imported->progress == 28);
  CHECK(imported->status == catalist::watchStatus::kCompleted);
  CHECK(imported->notes == "Masterpiece.");
}

TEST_CASE_FIXTURE(tempDirFixture, "malParser imports a manga block") {
  auto xmlPath = tempDir() / "manga.xml";
  catalist::test::writeTextFile(
      xmlPath,
      "<myanimelist>\n"
      "  <manga>\n"
      "    <manga_mangadb_id>2</manga_mangadb_id>\n"
      "    <manga_title><![CDATA[Berserk]]></manga_title>\n"
      "    <manga_volumes>0</manga_volumes>\n"
      "    <manga_chapters>0</manga_chapters>\n"
      "    <my_read_volumes>42</my_read_volumes>\n"
      "    <my_read_chapters>364</my_read_chapters>\n"
      "    <my_times_read>2</my_times_read>\n"
      "    <my_score>10</my_score>\n"
      "    <my_status>Reading</my_status>\n"
      "  </manga>\n"
      "</myanimelist>\n");

  catalist::database db(fileSystem(), clock(), tempDir());
  CHECK(catalist::malParser::importXml(fileSystem(), xmlPath.string(), db) == 1);
  auto imported = db.getById(catalist::mediaType::kManga, 2);
  REQUIRE(imported.has_value());
  CHECK(imported->type == catalist::mediaType::kManga);
  CHECK(imported->title == "Berserk");
  CHECK(imported->progress == 364);
  CHECK(imported->volumes == 42);
  CHECK(imported->rewatched == 2);
  CHECK(imported->status == catalist::watchStatus::kWatching);
  CHECK(imported->getRemoteId("mal") == "2");
  CHECK(db.getEntries(catalist::mediaType::kAnime).empty());
}

TEST_CASE_FIXTURE(tempDirFixture, "malParser decodes xml entities outside CDATA") {
  auto xmlPath = tempDir() / "entities.xml";
  catalist::test::writeTextFile(
      xmlPath,
      "<anime>\n"
      "  <series_animedb_id>1</series_animedb_id>\n"
      "  <series_title>Tom &amp; Jerry &lt;3 &quot;x&quot; &#039;y&apos;"
      "</series_title>\n"
      "  <my_status>Plan to Watch</my_status>\n"
      "</anime>\n");

  catalist::database db(fileSystem(), clock(), tempDir());
  CHECK(catalist::malParser::importXml(fileSystem(), xmlPath.string(), db) == 1);
  auto imported = db.getById(catalist::mediaType::kAnime, 1);
  REQUIRE(imported.has_value());
  CHECK(imported->title == "Tom & Jerry <3 \"x\" 'y'");
  CHECK(imported->status == catalist::watchStatus::kPlanToWatch);
}

TEST_CASE_FIXTURE(tempDirFixture, "malParser clean flag wipes only the imported media type") {
  catalist::database db(fileSystem(), clock(), tempDir());
  db.addEntry(catalist::test::makeEntry().id(500).title("Old anime"));
  db.addEntry(catalist::test::makeEntry().id(501).title("Old manga")
                  .type(catalist::mediaType::kManga));

  auto xmlPath = tempDir() / "clean.xml";
  catalist::test::writeTextFile(
      xmlPath,
      "<anime><series_animedb_id>9</series_animedb_id>"
      "<series_title>New</series_title></anime>\n");

  CHECK(catalist::malParser::importXml(fileSystem(), xmlPath.string(), db, true) == 1);
  CHECK_FALSE(db.getById(catalist::mediaType::kAnime, 500).has_value());
  CHECK(db.getById(catalist::mediaType::kAnime, 9).has_value());
  CHECK(db.getById(catalist::mediaType::kManga, 501).has_value());
}

TEST_CASE_FIXTURE(tempDirFixture, "malParser without clean merges into existing entries") {
  catalist::database db(fileSystem(), clock(), tempDir());
  db.addEntry(catalist::test::makeEntry().id(9).malId(9).title("Kept").score(7));

  auto xmlPath = tempDir() / "merge.xml";
  catalist::test::writeTextFile(
      xmlPath,
      "<anime><series_animedb_id>9</series_animedb_id>"
      "<series_title>Renamed</series_title><my_score>2</my_score></anime>\n");

  CHECK(catalist::malParser::importXml(fileSystem(), xmlPath.string(), db) == 1);
  auto merged = db.getById(catalist::mediaType::kAnime, 9);
  REQUIRE(merged.has_value());
  CHECK(merged->title == "Kept");
  CHECK(merged->score == 7);
}

TEST_CASE_FIXTURE(tempDirFixture, "malParser returns zero for a missing file") {
  catalist::database db(fileSystem(), clock(), tempDir());
  CHECK(catalist::malParser::importXml(fileSystem(), (tempDir() / "absent.xml").string(),
                                       db) == 0);
}
