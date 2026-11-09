#include <doctest/doctest.h>

#include "catalist/model/activity.hpp"
#include "support/testSupport.hpp"

using catalist::activityDay;
using catalist::activityRecord;
using catalist::test::requireDate;

namespace {

activityRecord stamped(const std::string& timestamp) {
  activityRecord record;
  record.timestamp = timestamp;
  return record;
}

}  // namespace

TEST_CASE("activityDay reads iso timestamps") {
  auto day = activityDay(stamped("2026-03-04 21:15"), requireDate("2026-10-20"));
  REQUIRE(day.has_value());
  CHECK(day->toString() == "2026-03-04");
}

TEST_CASE("activityDay infers the year of legacy timestamps") {
  auto today = requireDate("2026-10-20");
  CHECK(activityDay(stamped("Oct 05, 03:12 PM"), today)->toString() ==
        "2026-10-05");
  CHECK(activityDay(stamped("Dec 24, 09:00 AM"), today)->toString() ==
        "2025-12-24");
}

TEST_CASE("activityDay rejects unreadable timestamps") {
  auto today = requireDate("2026-10-20");
  CHECK_FALSE(activityDay(stamped(""), today).has_value());
  CHECK_FALSE(activityDay(stamped("yesterday"), today).has_value());
}

TEST_CASE("activityTimestamp writes iso minutes from the clock") {
  auto clock = catalist::test::makeTestClock();
  CHECK(catalist::activityTimestamp(clock) == "2026-10-20 12:00");
}
