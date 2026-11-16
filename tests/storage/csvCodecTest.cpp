#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "storage/csvCodec.hpp"
#include "support/testSupport.hpp"

using catalist::formatDate;
using catalist::mediaType;
using catalist::watchStatus;
namespace csvCodec = catalist::csvCodec;

TEST_CASE("csvCodec reads the zero date sentinel as absent and writes it empty") {
	auto decoded =
	    csvCodec::decodeEntries("1,1,anime,Old,Completed,7,1,1,0,0,0,0000-00-00,,0000-00-00\n",
	                            mediaType::kAnime)
	        .value();
	REQUIRE(decoded.size() == 1);
	CHECK_FALSE(decoded[0].startDate.has_value());
	CHECK_FALSE(decoded[0].finishDate.has_value());
	CHECK_FALSE(decoded[0].updatedAt.has_value());

	auto encoded = csvCodec::encodeEntries(decoded);
	CHECK(encoded.find("0000-00-00") == std::string::npos);
	CHECK(encoded.find("\n1,1,anime,Old,Completed,7,1,1,0,0,0,,,,0,,0\n") != std::string::npos);
}

TEST_CASE("csvCodec escapeField quotes only when needed") {
	CHECK(csvCodec::escapeField("plain") == "plain");
	CHECK(csvCodec::escapeField("a,b") == "\"a,b\"");
	CHECK(csvCodec::escapeField("say \"hi\"") == "\"say \"\"hi\"\"\"");
	CHECK(csvCodec::escapeField("two\nlines") == "\"two\nlines\"");
}

TEST_CASE("csvCodec splitRecords joins quoted newlines and skips comments") {
	auto records =
	    csvCodec::splitRecords("# header\r\n\r\na,\"multi\r\nline\",b\r\n# between\nc,d\n");
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
	CHECK_FALSE(
	    csvCodec::decodeEntry({"x", "0", "T", "Watching", "0", "0", "0"}, mediaType::kAnime));
	// The type column in the right place, but too few columns after it.
	CHECK_FALSE(csvCodec::decodeEntry({"1", "0", "anime", "T", "Watching"}, mediaType::kAnime));
	CHECK_FALSE(csvCodec::decodeEntry(csvCodec::splitFields("x,0,anime,T,Watching,0,0,0,0,0,0,,,"),
	                                  mediaType::kAnime));
}

TEST_CASE("csvCodec writes the 17 columns and reads them back") {
	catalist::entry original = catalist::test::makeEntry()
	                               .id(5)
	                               .malId(6)
	                               .anilistId(306)
	                               .title("R, \"t\"")
	                               .type(mediaType::kManga)
	                               .status(watchStatus::kOnHold)
	                               .score(4)
	                               .progress(3)
	                               .totalUnits(9)
	                               .volumes(1)
	                               .totalVolumes(2)
	                               .rewatched(1)
	                               .startDate("2024-01-02")
	                               .updatedAt("2024-02-03")
	                               .released(7)
	                               .checkedAt("2024-02-05");
	auto text = csvCodec::encodeEntries({original});
	CHECK(text ==
	      "#id,mal_id,type,title,status,score,progress,total_units,volumes,"
	      "total_volumes,rewatched,start_date,finish_date,updated_at,released,"
	      "checked_at,anilist_id\n"
	      "5,6,manga,\"R, \"\"t\"\"\",On-Hold,4,3,9,1,2,1,2024-01-02,,"
	      "2024-02-03,7,2024-02-05,306\n");
	auto decoded = csvCodec::decodeEntries(text, mediaType::kManga).value();
	REQUIRE(decoded.size() == 1);
	const auto& copy = decoded[0];
	CHECK(copy.id == 5);
	CHECK(copy.malId == 6);
	CHECK(copy.anilistId == 306);
	CHECK(copy.title == original.title);
	CHECK(copy.type == mediaType::kManga);
	CHECK(copy.status == watchStatus::kOnHold);
	CHECK(copy.score == 4);
	CHECK(copy.progress == 3);
	CHECK(copy.totalUnits == 9);
	CHECK(copy.volumes == 1);
	CHECK(copy.totalVolumes == 2);
	CHECK(copy.rewatched == 1);
	CHECK(formatDate(copy.startDate) == "2024-01-02");
	CHECK_FALSE(copy.finishDate.has_value());
	CHECK(formatDate(copy.updatedAt) == "2024-02-03");
	CHECK(copy.released == 7);
	CHECK(formatDate(copy.checkedAt) == "2024-02-05");
}

TEST_CASE("csvCodec reads rows from before the web check columns") {
	auto decoded = csvCodec::decodeEntries("1,2,manga,Older,Reading,0,5,0,0,0,0,,,2026-01-02\n",
	                                       mediaType::kManga)
	                   .value();
	REQUIRE(decoded.size() == 1);
	CHECK(decoded[0].title == "Older");
	CHECK(decoded[0].progress == 5);
	CHECK(formatDate(decoded[0].updatedAt) == "2026-01-02");
	CHECK(decoded[0].released == 0);
	CHECK_FALSE(decoded[0].checkedAt.has_value());
}

TEST_CASE("csvCodec reads the 0.3 layout and its extra columns, dropping them") {
	auto decoded = csvCodec::decodeEntries(
	                   "#id,mal_id,anilist_id,type,title,status,score,progress,total_units,"
	                   "volumes,total_volumes,rewatched,start_date,finish_date,notes,updated_at,"
	                   "dirty,external_ids,queue_pos,synced\n"
	                   "5,5,105,anime,Kept,Watching,7,3,12,0,0,2,2024-01-02,,\"a, \"\"b\"\"\","
	                   "2024-02-03,1,kitsu=9;mal=5,2,anilist;mal\n"
	                   "6,0,0,anime,Short,Dropped,0,1,0,0,0,0,,2024-03-04,,\n",
	                   mediaType::kAnime)
	                   .value();
	REQUIRE(decoded.size() == 2);
	CHECK(decoded[0].title == "Kept");
	CHECK(decoded[0].anilistId == 105);
	CHECK(decoded[0].score == 7);
	CHECK(decoded[0].rewatched == 2);
	CHECK(formatDate(decoded[0].startDate) == "2024-01-02");
	CHECK(formatDate(decoded[0].updatedAt) == "2024-02-03");
	CHECK(decoded[1].status == watchStatus::kDropped);
	CHECK(formatDate(decoded[1].finishDate) == "2024-03-04");
	CHECK(csvCodec::encodeEntries(decoded) ==
	      "#id,mal_id,type,title,status,score,progress,total_units,volumes,"
	      "total_volumes,rewatched,start_date,finish_date,updated_at,released,"
	      "checked_at,anilist_id\n"
	      "5,5,anime,Kept,Watching,7,3,12,0,0,2,2024-01-02,,2024-02-03,0,,105\n"
	      "6,0,anime,Short,Dropped,0,1,0,0,0,0,,2024-03-04,,0,,0\n");
}

TEST_CASE("csvCodec reads an unknown status as planned instead of dropping the row") {
	auto decoded = csvCodec::decodeEntries("1,0,manga,Edited by hand,Someday,0,0,0,0,0,0,,,\n",
	                                       mediaType::kManga);
	REQUIRE(decoded.ok());
	REQUIRE(decoded.value().size() == 1);
	CHECK(decoded.value()[0].status == watchStatus::kPlanToWatch);
	CHECK(decoded.value()[0].type == mediaType::kManga);
}

TEST_CASE("csvCodec names the first row it cannot read") {
	auto decoded = csvCodec::decodeEntries(
	    "1,0,anime,Fine,Watching,0,0,0,0,0,0,,,\n"
	    "x,0,anime,Bad id,Watching,0,0,0,0,0,0,,,\n",
	    mediaType::kAnime);
	REQUIRE_FALSE(decoded.ok());
	CHECK(decoded.errorMessage() == "row 2 has a column layout this version cannot read");
}

TEST_CASE("csvCodec adds the Kitsu id as an 18th field, only when there is one") {
	catalist::entry known =
	    catalist::test::makeEntry().id(1).title("Frieren").malId(52991).kitsuId(46474);
	std::string row = csvCodec::encodeEntry(known);
	auto fields = csvCodec::splitFields(row);
	REQUIRE(fields.size() == 18);
	CHECK(fields[17] == "46474");
	CHECK(csvCodec::decodeEntry(fields, mediaType::kAnime)->kitsuId == 46474);
	// Without one, the row is the 17 fields Catalist-TUI writes.
	known.kitsuId = 0;
	auto plain = csvCodec::splitFields(csvCodec::encodeEntry(known));
	CHECK(plain.size() == 17);
	CHECK(csvCodec::decodeEntry(plain, mediaType::kAnime)->kitsuId == 0);
}
