#include <doctest/doctest.h>

#include "catalist/model/activity.hpp"
#include "support/testSupport.hpp"

TEST_CASE("activityTimestamp writes iso minutes from the clock") {
  auto clock = catalist::test::makeTestClock();
  CHECK(catalist::activityTimestamp(clock) == "2026-10-20 12:00");
}
