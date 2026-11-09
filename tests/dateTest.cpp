#include <doctest/doctest.h>

#include "catalist/core/date.hpp"

using catalist::isoDate;

TEST_CASE("isoDate parses a valid date") {
  auto parsed = isoDate::parse("2024-03-22");
  REQUIRE(parsed.has_value());
  CHECK(parsed->year() == 2024);
  CHECK(parsed->month() == 3);
  CHECK(parsed->day() == 22);
  CHECK(parsed->toString() == "2024-03-22");
}

TEST_CASE("isoDate rejects the zero sentinel and short input") {
  CHECK_FALSE(isoDate::parse("0000-01-01").has_value());
  CHECK_FALSE(isoDate::parse("2024-03").has_value());
  CHECK_FALSE(isoDate::parse("").has_value());
}

// Characterization: pins current (buggy) behaviour, see roadmap Found issues.
TEST_CASE("isoDate accepts out of range month and day") {
  auto parsed = isoDate::parse("2024-13-45");
  REQUIRE(parsed.has_value());
  CHECK(parsed->month() == 13);
  CHECK(parsed->day() == 45);
}

TEST_CASE("isoDate day numbers round trip across leap years and eras") {
  CHECK(isoDate::parse("1970-01-01")->dayNumber() == 0);
  CHECK(isoDate::parse("2000-03-01")->dayNumber() == 11017);
  CHECK(isoDate::fromDayNumber(-1).toString() == "1969-12-31");
  for (const char* text : {"2024-02-29", "1900-03-01", "2100-12-31",
                           "0001-01-01", "2026-10-20"}) {
    auto date = isoDate::parse(text);
    REQUIRE(date.has_value());
    CHECK(isoDate::fromDayNumber(date->dayNumber()).toString() == text);
  }
}

TEST_CASE("isoDate weekday and addDays") {
  CHECK(isoDate::parse("1970-01-01")->weekday() == 3);
  CHECK(isoDate::parse("2026-10-19")->weekday() == 0);
  CHECK(isoDate::parse("2026-10-25")->weekday() == 6);
  CHECK(isoDate::parse("2024-02-28")->addDays(1).toString() == "2024-02-29");
  CHECK(isoDate::parse("2025-01-01")->addDays(-1).toString() == "2024-12-31");
}
