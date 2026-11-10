#include <doctest/doctest.h>

#include "catalist/storage/csvCodec.hpp"
#include "support/testSupport.hpp"

using catalist::formatDate;
using catalist::mediaType;
using catalist::watchStatus;
namespace csvCodec = catalist::csvCodec;

TEST_CASE("csvCodec reads the zero date sentinel as absent and writes it empty") {
  auto decoded = csvCodec::decodeEntries(
      "1,1,0,anime,Old,Completed,7,1,1,0,0,0,0000-00-00,,n,0000-00-00,0,\n",
      mediaType::kAnime).value();
  REQUIRE(decoded.size() == 1);
  CHECK_FALSE(decoded[0].startDate.has_value());
  CHECK_FALSE(decoded[0].finishDate.has_value());
  CHECK_FALSE(decoded[0].updatedAt.has_value());

  auto encoded = csvCodec::encodeEntries(decoded);
  CHECK(encoded.find("0000-00-00") == std::string::npos);
  CHECK(encoded.find("1,1,0,anime,Old,Completed,7,1,1,0,0,0,,,n,\n") !=
        std::string::npos);
}

TEST_CASE("csvCodec escapeField quotes only when needed") {
  CHECK(csvCodec::escapeField("plain") == "plain");
  CHECK(csvCodec::escapeField("a,b") == "\"a,b\"");
  CHECK(csvCodec::escapeField("say \"hi\"") == "\"say \"\"hi\"\"\"");
  CHECK(csvCodec::escapeField("two\nlines") == "\"two\nlines\"");
}

TEST_CASE("csvCodec splitRecords joins quoted newlines and skips comments") {
  auto records = csvCodec::splitRecords(
      "# header\r\n\r\na,\"multi\r\nline\",b\r\n# between\nc,d\n");
  REQUIRE(records.size() == 2);
  CHECK(records[0] == "a,\"multi\nline\",b");
  CHECK(records[1] == "c,d");
}

TEST_CASE("csvCodec splitFields handles escaped quotes and empty fields") {
  auto fields = csvCodec::splitFields("1,,\"x \"\"y\"\", z\",");
  REQUIRE(fields.size() == 4);
  CHECK(fields[1] == "");
  CHECK(fields[2] == "x \"y\", z");
  CHECK(fields[3] == "");
}

TEST_CASE("csvCodec decodeEntry rejects short rows and non numeric ids") {
  CHECK_FALSE(csvCodec::decodeEntry({"1", "2", "3"}, mediaType::kAnime));
  CHECK_FALSE(csvCodec::decodeEntry({"x", "0", "T", "Watching", "0", "0", "0"},
                                    mediaType::kAnime));
}

TEST_CASE("csvCodec entries round trip through encode and decode") {
  catalist::entry original = catalist::test::makeEntry()
                                 .id(5).malId(6).anilistId(7).title("R, \"t\"")
                                 .type(mediaType::kManga)
                                 .status(watchStatus::kOnHold).score(4)
                                 .progress(3).totalUnits(9).volumes(1)
                                 .totalVolumes(2).rewatched(1)
                                 .startDate("2024-01-02").notes("n\nn")
                                 .updatedAt("2024-02-03");
  auto decoded =
      csvCodec::decodeEntries(csvCodec::encodeEntries({original}),
                              mediaType::kManga).value();
  REQUIRE(decoded.size() == 1);
  const auto& copy = decoded[0];
  CHECK(copy.title == original.title);
  CHECK(copy.status == watchStatus::kOnHold);
  CHECK(copy.totalVolumes == 2);
  CHECK(formatDate(copy.startDate) == "2024-01-02");
  CHECK_FALSE(copy.finishDate.has_value());
  CHECK(copy.notes == "n\nn");
  CHECK(copy.malId == 6);
  CHECK(copy.anilistId == 7);
}

TEST_CASE("csvCodec reads files with the old internal columns and drops them") {
  auto decoded = csvCodec::decodeEntries(
      "#id,mal_id,anilist_id,type,title,status,score,progress,total_units,"
      "volumes,total_volumes,rewatched,start_date,finish_date,notes,updated_at,"
      "dirty,external_ids,queue_pos,synced\n"
      "5,5,105,anime,Kept,Watching,7,3,12,0,0,0,2024-01-02,,n,2024-02-03,1,"
      "kitsu=9;mal=5,2,anilist;mal\n",
      mediaType::kAnime).value();
  REQUIRE(decoded.size() == 1);
  CHECK(decoded[0].anilistId == 105);
  CHECK(decoded[0].notes == "n");
  CHECK(formatDate(decoded[0].updatedAt) == "2024-02-03");
  CHECK(csvCodec::encodeEntries(decoded) ==
        "#id,mal_id,anilist_id,type,title,status,score,progress,total_units,"
        "volumes,total_volumes,rewatched,start_date,finish_date,notes,updated_at\n"
        "5,5,105,anime,Kept,Watching,7,3,12,0,0,0,2024-01-02,,n,2024-02-03\n");
}

TEST_CASE("csvCodec reads an unknown status as planned instead of dropping the row") {
  auto decoded = csvCodec::decodeEntries(
      "1,0,0,manga,Edited by hand,Someday,0,0,0,0,0,0,,,,\n",
      mediaType::kManga);
  REQUIRE(decoded.ok());
  REQUIRE(decoded.value().size() == 1);
  CHECK(decoded.value()[0].status == watchStatus::kPlanToWatch);
  CHECK(decoded.value()[0].type == mediaType::kManga);
}

TEST_CASE("csvCodec names the first row it cannot read") {
  auto decoded = csvCodec::decodeEntries(
      "1,0,0,anime,Fine,Watching,0,0,0,0,0,0,,,,\n"
      "x,0,0,anime,Bad id,Watching,0,0,0,0,0,0,,,,\n",
      mediaType::kAnime);
  REQUIRE_FALSE(decoded.ok());
  CHECK(decoded.errorMessage() ==
        "row 2 has a column layout this version cannot read");
}
