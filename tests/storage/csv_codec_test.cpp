#include "storage/csv_codec.hpp"

#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "support/test_support.hpp"

using catalist::FormatDate;
using catalist::MediaType;
using catalist::WatchStatus;
namespace csv_codec = catalist::csv_codec;

TEST_CASE("csvCodec escapeField quotes only when needed") {
	CHECK(csv_codec::EscapeField("plain") == "plain");
	CHECK(csv_codec::EscapeField("a,b") == "\"a,b\"");
	CHECK(csv_codec::EscapeField("say \"hi\"") == "\"say \"\"hi\"\"\"");
	CHECK(csv_codec::EscapeField("two\nlines") == "\"two\nlines\"");
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
	CHECK_FALSE(csv_codec::DecodeEntry({"1", "0", "anime", "T", "Watching"}));
	CHECK_FALSE(
	    csv_codec::DecodeEntry(csv_codec::SplitFields("x,0,anime,T,Watching,0,0,0,0,0,0,,,,0")));
	CHECK_FALSE(
	    csv_codec::DecodeEntry(csv_codec::SplitFields("1,0,book,T,Watching,0,0,0,0,0,0,,,,0")));
}

TEST_CASE("csvCodec writes a row and reads it back") {
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
	                               .UpdatedAt("2024-02-03");
	auto row = csv_codec::EncodeEntry(original);
	CHECK(row ==
	      "5,6,manga,\"R, \"\"t\"\"\",On-Hold,4,3,9,1,2,1,2024-01-02,,"
	      "2024-02-03,306");
	auto copy = csv_codec::DecodeEntry(csv_codec::SplitFields(row));
	REQUIRE(copy.has_value());
	CHECK(*copy == original);
	CHECK(FormatDate(copy->start_date) == "2024-01-02");
	CHECK_FALSE(copy->finish_date.has_value());
}

TEST_CASE("csvCodec reads an unknown status as planned instead of dropping the row") {
	auto decoded = csv_codec::DecodeEntry(
	    csv_codec::SplitFields("1,0,manga,Edited by hand,Someday,0,0,0,0,0,0,,,,0"));
	REQUIRE(decoded.has_value());
	CHECK(decoded->status == WatchStatus::kPlanToWatch);
	CHECK(decoded->type == MediaType::kManga);
}
