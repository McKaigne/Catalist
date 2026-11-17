#include "storage/csv_codec.hpp"

#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "support/test_support.hpp"

using catalist::FormatDate;
using catalist::MediaType;
using catalist::WatchStatus;
namespace csv_codec = catalist::csv_codec;

TEST_CASE("csvCodec reads the zero date sentinel as absent and writes it empty") {
	auto decoded =
	    csv_codec::DecodeEntries("1,1,anime,Old,Completed,7,1,1,0,0,0,0000-00-00,,0000-00-00\n")
	        .value();
	REQUIRE(decoded.size() == 1);
	CHECK_FALSE(decoded[0].start_date.has_value());
	CHECK_FALSE(decoded[0].finish_date.has_value());
	CHECK_FALSE(decoded[0].updated_at.has_value());

	CHECK(csv_codec::EncodeEntry(decoded[0]) == "1,1,anime,Old,Completed,7,1,1,0,0,0,,,,0,,0");
}

TEST_CASE("csvCodec escapeField quotes only when needed") {
	CHECK(csv_codec::EscapeField("plain") == "plain");
	CHECK(csv_codec::EscapeField("a,b") == "\"a,b\"");
	CHECK(csv_codec::EscapeField("say \"hi\"") == "\"say \"\"hi\"\"\"");
	CHECK(csv_codec::EscapeField("two\nlines") == "\"two\nlines\"");
}

TEST_CASE("csvCodec splitRecords joins quoted newlines and skips comments") {
	auto records =
	    csv_codec::SplitRecords("# header\r\n\r\na,\"multi\r\nline\",b\r\n# between\nc,d\n");
	REQUIRE(records.size() == 2);
	CHECK(records[0] == "a,\"multi\nline\",b");
	CHECK(records[1] == "c,d");
}

TEST_CASE("csvCodec splitFields handles escaped quotes and empty fields") {
	auto fields = csv_codec::SplitFields("1,,\"x \"\"y\"\", z\",");
	REQUIRE(fields.size() == 4);
	CHECK(fields[1] == "");
	CHECK(fields[2] == "x \"y\", z");
	CHECK(fields[3] == "");
}

TEST_CASE("csvCodec decodeEntry rejects short rows and non numeric ids") {
	CHECK_FALSE(csv_codec::DecodeEntry({"1", "2", "3"}));
	CHECK_FALSE(csv_codec::DecodeEntry({"x", "0", "T", "Watching", "0", "0", "0"}));
	// The type column in the right place, but too few columns after it.
	CHECK_FALSE(csv_codec::DecodeEntry({"1", "0", "anime", "T", "Watching"}));
	CHECK_FALSE(
	    csv_codec::DecodeEntry(csv_codec::SplitFields("x,0,anime,T,Watching,0,0,0,0,0,0,,,")));
}

TEST_CASE("csvCodec writes the 17 columns and reads them back") {
	catalist::Entry original = catalist::test::MakeEntry()
	                               .Id(5)
	                               .MalId(6)
	                               .AnilistId(306)
	                               .Title("R, \"t\"")
	                               .Type(MediaType::kManga)
	                               .Status(WatchStatus::kOnHold)
	                               .Score(4)
	                               .Progress(3)
	                               .TotalUnits(9)
	                               .Volumes(1)
	                               .TotalVolumes(2)
	                               .Rewatched(1)
	                               .StartDate("2024-01-02")
	                               .UpdatedAt("2024-02-03")
	                               .Released(7)
	                               .CheckedAt("2024-02-05");
	auto row = csv_codec::EncodeEntry(original);
	CHECK(row ==
	      "5,6,manga,\"R, \"\"t\"\"\",On-Hold,4,3,9,1,2,1,2024-01-02,,"
	      "2024-02-03,7,2024-02-05,306");
	auto decoded = csv_codec::DecodeEntries(
	                   "#id,mal_id,type,title,status,score,progress,total_units,volumes,"
	                   "total_volumes,rewatched,start_date,finish_date,updated_at,released,"
	                   "checked_at,anilist_id\n" +
	                   row + "\n")
	                   .value();
	REQUIRE(decoded.size() == 1);
	const auto& copy = decoded[0];
	CHECK(copy.id == 5);
	CHECK(copy.mal_id == 6);
	CHECK(copy.anilist_id == 306);
	CHECK(copy.title == original.title);
	CHECK(copy.type == MediaType::kManga);
	CHECK(copy.status == WatchStatus::kOnHold);
	CHECK(copy.score == 4);
	CHECK(copy.progress == 3);
	CHECK(copy.total_units == 9);
	CHECK(copy.volumes == 1);
	CHECK(copy.total_volumes == 2);
	CHECK(copy.rewatched == 1);
	CHECK(FormatDate(copy.start_date) == "2024-01-02");
	CHECK_FALSE(copy.finish_date.has_value());
	CHECK(FormatDate(copy.updated_at) == "2024-02-03");
	CHECK(copy.released == 7);
	CHECK(FormatDate(copy.checked_at) == "2024-02-05");
}

TEST_CASE("csvCodec reads rows from before the web check columns") {
	auto decoded =
	    csv_codec::DecodeEntries("1,2,manga,Older,Reading,0,5,0,0,0,0,,,2026-01-02\n").value();
	REQUIRE(decoded.size() == 1);
	CHECK(decoded[0].title == "Older");
	CHECK(decoded[0].progress == 5);
	CHECK(FormatDate(decoded[0].updated_at) == "2026-01-02");
	CHECK(decoded[0].released == 0);
	CHECK_FALSE(decoded[0].checked_at.has_value());
}

TEST_CASE("csvCodec reads the 0.3 layout and its extra columns, dropping them") {
	auto decoded = csv_codec::DecodeEntries(
	                   "#id,mal_id,anilist_id,type,title,status,score,progress,total_units,"
	                   "volumes,total_volumes,rewatched,start_date,finish_date,notes,updated_at,"
	                   "dirty,external_ids,queue_pos,synced\n"
	                   "5,5,105,anime,Kept,Watching,7,3,12,0,0,2,2024-01-02,,\"a, \"\"b\"\"\","
	                   "2024-02-03,1,kitsu=9;mal=5,2,anilist;mal\n"
	                   "6,0,0,anime,Short,Dropped,0,1,0,0,0,0,,2024-03-04,,\n")
	                   .value();
	REQUIRE(decoded.size() == 2);
	CHECK(decoded[0].title == "Kept");
	CHECK(decoded[0].anilist_id == 105);
	CHECK(decoded[0].score == 7);
	CHECK(decoded[0].rewatched == 2);
	CHECK(FormatDate(decoded[0].start_date) == "2024-01-02");
	CHECK(FormatDate(decoded[0].updated_at) == "2024-02-03");
	CHECK(decoded[1].status == WatchStatus::kDropped);
	CHECK(FormatDate(decoded[1].finish_date) == "2024-03-04");
	CHECK(csv_codec::EncodeEntry(decoded[0]) ==
	      "5,5,anime,Kept,Watching,7,3,12,0,0,2,2024-01-02,,2024-02-03,0,,105");
	CHECK(csv_codec::EncodeEntry(decoded[1]) ==
	      "6,0,anime,Short,Dropped,0,1,0,0,0,0,,2024-03-04,,0,,0");
}

TEST_CASE("csvCodec reads an unknown status as planned instead of dropping the row") {
	auto decoded = csv_codec::DecodeEntries("1,0,manga,Edited by hand,Someday,0,0,0,0,0,0,,,\n");
	REQUIRE(decoded.has_value());
	REQUIRE(decoded.value().size() == 1);
	CHECK(decoded.value()[0].status == WatchStatus::kPlanToWatch);
	CHECK(decoded.value()[0].type == MediaType::kManga);
}

TEST_CASE("csvCodec names the first row it cannot read") {
	auto decoded = csv_codec::DecodeEntries(
	    "1,0,anime,Fine,Watching,0,0,0,0,0,0,,,\n"
	    "x,0,anime,Bad id,Watching,0,0,0,0,0,0,,,\n");
	REQUIRE_FALSE(decoded.has_value());
	CHECK(decoded.error() == "row 2 has a column layout this version cannot read");
}

TEST_CASE("csvCodec adds the Kitsu id as an 18th field, only when there is one") {
	catalist::Entry known =
	    catalist::test::MakeEntry().Id(1).Title("Frieren").MalId(52991).KitsuId(46474);
	std::string row = csv_codec::EncodeEntry(known);
	auto fields = csv_codec::SplitFields(row);
	REQUIRE(fields.size() == 18);
	CHECK(fields[17] == "46474");
	CHECK(csv_codec::DecodeEntry(fields)->kitsu_id == 46474);
	// Without one, the row is the 17 fields Catalist-TUI writes.
	known.kitsu_id = 0;
	auto plain = csv_codec::SplitFields(csv_codec::EncodeEntry(known));
	CHECK(plain.size() == 17);
	CHECK(csv_codec::DecodeEntry(plain)->kitsu_id == 0);
}
