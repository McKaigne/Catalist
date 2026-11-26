#include "storage/csv_codec.hpp"

#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "support/test_support.hpp"

using catalist::MediaType;
using catalist::WatchStatus;
using catalist::test::MakeEntry;
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

TEST_CASE("csvCodec splits records, keeping a line break inside quotes") {
	CHECK(csv_codec::SplitRecords("a,b\r\n\"c\nd\",e\n\nf") ==
	      std::vector<std::string>{"a,b", "\"c\nd\",e", "f"});
	CHECK(csv_codec::SplitRecords("").empty());
}

TEST_CASE("csvCodec writes a row and reads it back") {
	catalist::Entry original = MakeEntry()
	                               .Id(5)
	                               .MalId(6)
	                               .AnilistId(306)
	                               .Title("R, \"t\"")
	                               .EnglishTitle("English")
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
	original.changed_at = "2024-02-03 10:00:00";
	original.rewatch_value = catalist::RewatchValue::kVeryHigh;
	auto row = csv_codec::EncodeEntry(original);
	CHECK(row ==
	      "5,manga,\"R, \"\"t\"\"\",English,on-hold,4,3,9,1,2,1,2024-01-02,,2024-02-03,"
	      "2024-02-03 10:00:00,6,306,very-high");
	auto copy = csv_codec::DecodeEntry(csv_codec::SplitFields(row));
	REQUIRE(copy.has_value());
	CHECK(*copy == original);
}

TEST_CASE("csvCodec reads every status it writes") {
	for (auto status : {WatchStatus::kWatching, WatchStatus::kCompleted, WatchStatus::kOnHold,
	                    WatchStatus::kDropped, WatchStatus::kPlanToWatch, WatchStatus::kRewatching}) {
		catalist::Entry e = MakeEntry().Id(1).Title("T").Status(status);
		auto copy = csv_codec::DecodeEntry(csv_codec::SplitFields(csv_codec::EncodeEntry(e)));
		REQUIRE(copy.has_value());
		CHECK(copy->status == status);
	}
}

TEST_CASE("csvCodec reads a row from before the rewatch value") {
	auto old = csv_codec::DecodeEntry(csv_codec::SplitFields("1,anime,T,,planned,0,0,0,0,0,0,,,,,0,0"));
	REQUIRE(old.has_value());
	CHECK(old->rewatch_value == catalist::RewatchValue::kNone);
	CHECK_FALSE(csv_codec::DecodeEntry(
	                csv_codec::SplitFields("1,anime,T,,planned,0,0,0,0,0,0,,,,,0,0,lots"))
	                .has_value());
}

TEST_CASE("csvCodec refuses a damaged row instead of guessing") {
	const std::string good = "1,anime,T,,planned,0,0,0,0,0,0,,,,,0,0";
	CHECK(csv_codec::DecodeEntry(csv_codec::SplitFields(good)).has_value());
	for (const char* bad : {"1,2,3", "x,anime,T,,planned,0,0,0,0,0,0,,,,,0,0",
	                        "1,book,T,,planned,0,0,0,0,0,0,,,,,0,0",
	                        "1,anime,,,planned,0,0,0,0,0,0,,,,,0,0",
	                        "1,anime,T,,someday,0,0,0,0,0,0,,,,,0,0",
	                        "1,anime,T,,planned,11,0,0,0,0,0,,,,,0,0",
	                        "1,anime,T,,planned,0,-1,0,0,0,0,,,,,0,0",
	                        "1,anime,T,,planned,0,0,0,0,0,0,2026-13-40,,,,0,0"})
		CHECK_FALSE(csv_codec::DecodeEntry(csv_codec::SplitFields(bad)).has_value());
}
