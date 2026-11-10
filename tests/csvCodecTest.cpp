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
      mediaType::kAnime);
  REQUIRE(decoded.size() == 1);
  CHECK_FALSE(decoded[0].startDate.has_value());
  CHECK_FALSE(decoded[0].finishDate.has_value());
  CHECK_FALSE(decoded[0].updatedAt.has_value());

  auto encoded = csvCodec::encodeEntries(decoded);
  CHECK(encoded.find("0000-00-00") == std::string::npos);
  CHECK(encoded.find("1,1,0,anime,Old,Completed,7,1,1,0,0,0,,,n,,,,,\n") !=
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
                              mediaType::kManga);
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

TEST_CASE("csvCodec history round trips and skips short rows") {
  std::vector<catalist::activityRecord> history = {
      {"Oct 20, 12:00 PM", mediaType::kManga, "A, b", watchStatus::kWatching,
       4, 10, 8}};
  auto text = csvCodec::encodeHistory(history) + "too,short\n";
  auto decoded = csvCodec::decodeHistory(text);
  REQUIRE(decoded.size() == 1);
  CHECK(decoded[0].timestamp == "Oct 20, 12:00 PM");
  CHECK(decoded[0].type == mediaType::kManga);
  CHECK(decoded[0].title == "A, b");
  CHECK(decoded[0].score == 8);
}

TEST_CASE("csvCodec queue position round trips and is absent in older files") {
  catalist::entry queued = catalist::test::makeEntry().id(3).title("Q");
  queued.queuePos = 4;
  auto decoded = csvCodec::decodeEntries(csvCodec::encodeEntries({queued}),
                                         mediaType::kAnime);
  REQUIRE(decoded.size() == 1);
  CHECK(decoded[0].queuePos == 4);

  auto legacy = csvCodec::decodeEntries(
      "3,0,0,anime,Q,Watching,0,0,0,0,0,0,,,,,0,\n", mediaType::kAnime);
  REQUIRE(legacy.size() == 1);
  CHECK_FALSE(legacy[0].queuePos.has_value());
}

TEST_CASE("csvCodec round trips sync bases") {
  catalist::entry values = catalist::test::makeEntry()
                               .type(mediaType::kManga)
                               .status(watchStatus::kOnHold).score(7)
                               .progress(30).volumes(3).rewatched(1)
                               .startDate("2024-01-02").notes("a, \"b\"\nc");
  auto decoded = csvCodec::decodeSyncBases(
      csvCodec::encodeSyncBases({{"anilist", 30002, values}}));
  REQUIRE(decoded.size() == 1);
  CHECK(decoded[0].service == "anilist");
  CHECK(decoded[0].remoteId == 30002);
  const auto& copy = decoded[0].values;
  CHECK(copy.type == mediaType::kManga);
  CHECK(copy.status == watchStatus::kOnHold);
  CHECK(copy.score == 7);
  CHECK(copy.progress == 30);
  CHECK(copy.volumes == 3);
  CHECK(copy.rewatched == 1);
  CHECK(formatDate(copy.startDate) == "2024-01-02");
  CHECK_FALSE(copy.finishDate.has_value());
  CHECK(copy.notes == "a, \"b\"\nc");
}
