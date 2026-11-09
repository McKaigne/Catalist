#include <doctest/doctest.h>

#include "catalist/database.hpp"
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

  catalist::Database db(tempDir());
  CHECK(catalist::MalParser::importXml(xmlPath.string(), db, true) == 1);

  auto imported = db.getById(catalist::MediaType::Anime, 52991);
  REQUIRE(imported.has_value());
  CHECK(imported->title == "Sousou no Frieren");
  CHECK(imported->score == 10);
  CHECK(imported->progress == 28);
  CHECK(imported->status == catalist::WatchStatus::Completed);
  CHECK(imported->notes == "Masterpiece.");
}
