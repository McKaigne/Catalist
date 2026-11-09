#include <doctest/doctest.h>

#include "catalist/core/date.hpp"

using catalist::IsoDate;

TEST_CASE("isoDate parses a valid date") {
  auto parsed = IsoDate::parse("2024-03-22");
  REQUIRE(parsed.has_value());
  CHECK(parsed->year() == 2024);
  CHECK(parsed->month() == 3);
  CHECK(parsed->day() == 22);
  CHECK(parsed->toString() == "2024-03-22");
}

TEST_CASE("isoDate rejects the zero sentinel and short input") {
  CHECK_FALSE(IsoDate::parse("0000-00-00").has_value());
  CHECK_FALSE(IsoDate::parse("2024-03").has_value());
  CHECK_FALSE(IsoDate::parse("").has_value());
}

// Characterization: pins current (buggy) behaviour, see roadmap Found issues.
TEST_CASE("isoDate accepts out of range month and day") {
  auto parsed = IsoDate::parse("2024-13-45");
  REQUIRE(parsed.has_value());
  CHECK(parsed->month() == 13);
  CHECK(parsed->day() == 45);
}
