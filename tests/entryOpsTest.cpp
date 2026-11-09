#include <doctest/doctest.h>

#include "catalist/model/entryOps.hpp"
#include "support/testSupport.hpp"

using catalist::entry;
using catalist::formatDate;
using catalist::isoDate;
using catalist::watchStatus;
using catalist::test::makeEntry;
using catalist::test::requireDate;

namespace {

const isoDate kToday = requireDate("2026-10-20");

entry watching(int progress, int total) {
  return makeEntry().id(1).title("Show").status(watchStatus::kWatching)
      .progress(progress).totalUnits(total).startDate("2026-01-01");
}

}  // namespace

TEST_CASE("setProgress clamps at zero and completes at the total") {
  CHECK(catalist::setProgress(watching(5, 12), -3, kToday).progress == 0);

  auto finished = catalist::setProgress(watching(5, 12), 40, kToday);
  CHECK(finished.progress == 12);
  CHECK(finished.status == watchStatus::kCompleted);
  CHECK_FALSE(finished.finishDate.has_value());
  CHECK(formatDate(finished.updatedAt) == "2026-10-20");
}

TEST_CASE("setProgress with an unknown total never completes") {
  auto open = catalist::setProgress(watching(5, 0), 500, kToday);
  CHECK(open.progress == 500);
  CHECK(open.status == watchStatus::kWatching);
}

TEST_CASE("stepProgress forward starts a planned entry") {
  entry planned = makeEntry().status(watchStatus::kPlanToWatch).totalUnits(3);
  auto started = catalist::stepProgress(planned, 1, kToday);
  CHECK(started.progress == 1);
  CHECK(started.status == watchStatus::kWatching);
  CHECK(formatDate(started.startDate) == "2026-10-20");
  CHECK_FALSE(started.finishDate.has_value());
}

TEST_CASE("stepProgress forward to the total completes with a finish date") {
  auto finished = catalist::stepProgress(watching(11, 12), 1, kToday);
  CHECK(finished.status == watchStatus::kCompleted);
  CHECK(formatDate(finished.finishDate) == "2026-10-20");
  CHECK(formatDate(finished.startDate) == "2026-01-01");
}

TEST_CASE("stepProgress back below the total reopens a completed entry") {
  entry done = makeEntry().status(watchStatus::kCompleted).progress(12)
                   .totalUnits(12).finishDate("2026-05-05");
  auto reopened = catalist::stepProgress(done, -1, kToday);
  CHECK(reopened.progress == 11);
  CHECK(reopened.status == watchStatus::kWatching);
  CHECK_FALSE(reopened.finishDate.has_value());
}

TEST_CASE("stepProgress back stops at zero") {
  CHECK(catalist::stepProgress(watching(0, 12), -1, kToday).progress == 0);
}

TEST_CASE("cycleStatus walks the full cycle in both directions") {
  entry e = watching(0, 12);
  const watchStatus forward[] = {watchStatus::kCompleted, watchStatus::kOnHold,
                                 watchStatus::kDropped,
                                 watchStatus::kPlanToWatch,
                                 watchStatus::kWatching};
  for (auto expected : forward) {
    e = catalist::cycleStatus(e, 1, kToday);
    CHECK(e.status == expected);
  }
  e = catalist::cycleStatus(e, -1, kToday);
  CHECK(e.status == watchStatus::kPlanToWatch);
}

TEST_CASE("cycleStatus to completed fills progress and finish date") {
  auto done = catalist::cycleStatus(watching(4, 12), 1, kToday);
  CHECK(done.status == watchStatus::kCompleted);
  CHECK(done.progress == 12);
  CHECK(formatDate(done.finishDate) == "2026-10-20");
}

TEST_CASE("cycleStatus from completed back to watching clears the finish date") {
  entry done = makeEntry().status(watchStatus::kCompleted)
                   .finishDate("2026-05-05").startDate("2026-01-01");
  auto resumed = catalist::cycleStatus(done, -1, kToday);
  CHECK(resumed.status == watchStatus::kWatching);
  CHECK_FALSE(resumed.finishDate.has_value());
}

TEST_CASE("cycleStatus to on hold keeps an existing finish date") {
  entry dropped = makeEntry().status(watchStatus::kCompleted)
                      .finishDate("2026-05-05");
  auto onHold = catalist::cycleStatus(dropped, 1, kToday);
  CHECK(onHold.status == watchStatus::kOnHold);
  CHECK(formatDate(onHold.finishDate) == "2026-05-05");
}

TEST_CASE("adjustRating stays within zero and ten") {
  CHECK(catalist::adjustRating(makeEntry().score(9), 3, kToday).score == 10);
  CHECK(catalist::adjustRating(makeEntry().score(1), -3, kToday).score == 0);
  CHECK(formatDate(catalist::adjustRating(makeEntry(), 1, kToday).updatedAt) ==
        "2026-10-20");
}
