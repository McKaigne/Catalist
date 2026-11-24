#include "core/date.hpp"

#include <doctest/doctest.h>

using catalist::IsoDate;

TEST_CASE("isoDate parses a valid date") {
	auto parsed = IsoDate::Parse("2024-03-22");
	REQUIRE(parsed.has_value());
	CHECK(parsed->ToString() == "2024-03-22");
}

TEST_CASE("isoDate rejects the zero sentinel and short input") {
	CHECK_FALSE(IsoDate::Parse("0000-01-01").has_value());
	CHECK_FALSE(IsoDate::Parse("2024-03").has_value());
	CHECK_FALSE(IsoDate::Parse("").has_value());
}

TEST_CASE("isoDate rejects days that are not on the calendar") {
	CHECK_FALSE(IsoDate::Parse("2024-13-45").has_value());
	CHECK_FALSE(IsoDate::Parse("2024-00-10").has_value());
	CHECK_FALSE(IsoDate::Parse("2023-02-29").has_value());
	CHECK_FALSE(IsoDate::Parse("2024-04-31").has_value());
	CHECK(IsoDate::Parse("2024-02-29").has_value());
	CHECK(IsoDate::Parse("2000-02-29").has_value());
	CHECK_FALSE(IsoDate::Parse("1900-02-29").has_value());
}

TEST_CASE("isoDate rejects malformed text but accepts a trailing time") {
	CHECK_FALSE(IsoDate::Parse("2024/03/22").has_value());
	CHECK_FALSE(IsoDate::Parse("2024-3-22xx").has_value());
	CHECK_FALSE(IsoDate::Parse("2024-03-22abc").has_value());
	CHECK_FALSE(IsoDate::Parse("20a4-03-22").has_value());
	CHECK(IsoDate::Parse("2024-03-22T10:00:00+00:00")->ToString() == "2024-03-22");
	CHECK(IsoDate::Parse("2024-03-22 10:00")->ToString() == "2024-03-22");
}

TEST_CASE("isoDate fromParts validates each part") {
	CHECK(IsoDate::FromParts(2026, 11, 10)->ToString() == "2026-11-10");
	CHECK_FALSE(IsoDate::FromParts(2026, 11, 31).has_value());
	CHECK_FALSE(IsoDate::FromParts(0, 1, 1).has_value());
}

TEST_CASE("isoDate compares equal only on the same day") {
	CHECK(*IsoDate::Parse("2024-01-01") == *IsoDate::Parse("2024-01-01"));
	CHECK_FALSE(*IsoDate::Parse("2023-12-31") == *IsoDate::Parse("2024-01-01"));
}
