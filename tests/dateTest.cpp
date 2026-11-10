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

TEST_CASE("isoDate rejects days that are not on the calendar") {
  CHECK_FALSE(isoDate::parse("2024-13-45").has_value());
  CHECK_FALSE(isoDate::parse("2024-00-10").has_value());
  CHECK_FALSE(isoDate::parse("2023-02-29").has_value());
  CHECK_FALSE(isoDate::parse("2024-04-31").has_value());
  CHECK(isoDate::parse("2024-02-29").has_value());
  CHECK(isoDate::parse("2000-02-29").has_value());
  CHECK_FALSE(isoDate::parse("1900-02-29").has_value());
}

TEST_CASE("isoDate rejects malformed text but accepts a trailing time") {
  CHECK_FALSE(isoDate::parse("2024/03/22").has_value());
  CHECK_FALSE(isoDate::parse("2024-3-22xx").has_value());
  CHECK_FALSE(isoDate::parse("2024-03-22abc").has_value());
  CHECK_FALSE(isoDate::parse("20a4-03-22").has_value());
  CHECK(isoDate::parse("2024-03-22T10:00:00+00:00")->toString() == "2024-03-22");
  CHECK(isoDate::parse("2024-03-22 10:00")->toString() == "2024-03-22");
}

TEST_CASE("isoDate fromParts validates each part") {
  CHECK(isoDate::fromParts(2026, 11, 10)->toString() == "2026-11-10");
  CHECK_FALSE(isoDate::fromParts(2026, 11, 31).has_value());
  CHECK_FALSE(isoDate::fromParts(0, 1, 1).has_value());
}

TEST_CASE("isoDate orders dates by year, then month, then day") {
  auto early = *isoDate::parse("2023-12-31");
  auto late = *isoDate::parse("2024-01-01");
  CHECK(early < late);
  CHECK(late > early);
  CHECK(early != late);
  CHECK(*isoDate::parse("2024-01-01") == late);
  CHECK(*isoDate::parse("2024-01-02") > late);
  CHECK(*isoDate::parse("2024-02-01") > *isoDate::parse("2024-01-31"));
}
