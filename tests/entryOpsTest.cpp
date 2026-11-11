#include <doctest/doctest.h>

#include "catalist/model/entryOps.hpp"
#include "support/testSupport.hpp"

using catalist::countField;
using catalist::entry;
using catalist::formatDate;
using catalist::isoDate;
using catalist::watchStatus;
using catalist::test::makeEntry;
using catalist::test::requireDate;

namespace {

const isoDate kToday = requireDate("2026-10-20");

entry watching(int progress, int total) {
  return makeEntry()
      .id(1)
      .title("Show")
      .status(watchStatus::kWatching)
      .progress(progress)
      .totalUnits(total)
      .startDate("2026-01-01");
}

}  // namespace

TEST_CASE("stepProgress clamps at zero and at the total") {
  CHECK(catalist::stepProgress(watching(5, 12), -9, kToday).progress == 0);

  auto finished = catalist::stepProgress(watching(5, 12), 40, kToday);
  CHECK(finished.progress == 12);
  CHECK(finished.status == watchStatus::kCompleted);
  CHECK(formatDate(finished.finishDate) == "2026-10-20");
  CHECK(formatDate(finished.updatedAt) == "2026-10-20");
}

TEST_CASE("stepProgress with an unknown total never completes") {
  auto open = catalist::stepProgress(watching(5, 0), 495, kToday);
  CHECK(open.progress == 500);
  CHECK(open.status == watchStatus::kWatching);
  CHECK_FALSE(open.finishDate.has_value());
}

TEST_CASE("stepProgress leaves a finished or empty entry exactly as it was") {
  // Pressing + on a completed title used to stamp today as its start and
  // finish date.
  entry done = makeEntry()
                   .status(watchStatus::kCompleted)
                   .progress(28)
                   .totalUnits(28)
                   .updatedAt("2024-01-01");
  auto same = catalist::stepProgress(done, 1, kToday);
  CHECK_FALSE(same.startDate.has_value());
  CHECK_FALSE(same.finishDate.has_value());
  CHECK(formatDate(same.updatedAt) == "2024-01-01");

  auto atZero = catalist::stepProgress(watching(0, 12), -1, kToday);
  CHECK_FALSE(atZero.updatedAt.has_value());
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
  entry done = makeEntry()
                   .status(watchStatus::kCompleted)
                   .progress(12)
                   .totalUnits(12)
                   .finishDate("2026-05-05");
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
  const watchStatus forward[] = {
      watchStatus::kCompleted, watchStatus::kOnHold, watchStatus::kDropped,
      watchStatus::kPlanToWatch, watchStatus::kWatching};
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

TEST_CASE(
    "cycleStatus from completed back to watching clears the finish date") {
  entry done = makeEntry()
                   .status(watchStatus::kCompleted)
                   .finishDate("2026-05-05")
                   .startDate("2026-01-01");
  auto resumed = catalist::cycleStatus(done, -1, kToday);
  CHECK(resumed.status == watchStatus::kWatching);
  CHECK_FALSE(resumed.finishDate.has_value());
}

TEST_CASE("cycleStatus to on hold keeps an existing finish date") {
  entry dropped =
      makeEntry().status(watchStatus::kCompleted).finishDate("2026-05-05");
  auto onHold = catalist::cycleStatus(dropped, 1, kToday);
  CHECK(onHold.status == watchStatus::kOnHold);
  CHECK(formatDate(onHold.finishDate) == "2026-05-05");
}

TEST_CASE("adjustRating stays within zero and ten") {
  CHECK(catalist::adjustRating(makeEntry().score(9), 3, kToday).score == 10);
  CHECK(catalist::adjustRating(makeEntry().score(1), -3, kToday).score == 0);
  CHECK(formatDate(catalist::adjustRating(makeEntry(), 1, kToday).updatedAt) ==
        "2026-10-20");
  // Already at the limit: nothing changes, not even the date.
  CHECK_FALSE(catalist::adjustRating(makeEntry().score(10), 1, kToday)
                  .updatedAt.has_value());
}

TEST_CASE("setCount types progress in like a long step") {
  entry planned = makeEntry().status(watchStatus::kPlanToWatch).totalUnits(24);
  auto started = catalist::setCount(planned, countField::kProgress, 10, kToday);
  CHECK(started.progress == 10);
  CHECK(started.status == watchStatus::kWatching);
  CHECK(formatDate(started.startDate) == "2026-10-20");

  auto done = catalist::setCount(started, countField::kProgress, 99, kToday);
  CHECK(done.progress == 24);
  CHECK(done.status == watchStatus::kCompleted);

  auto reopened = catalist::setCount(done, countField::kProgress, 3, kToday);
  CHECK(reopened.progress == 3);
  CHECK(reopened.status == watchStatus::kWatching);
  CHECK_FALSE(reopened.finishDate.has_value());
}

TEST_CASE("setCount sets totals, volumes and rewatches within bounds") {
  entry show = watching(10, 0);
  auto withTotal =
      catalist::setCount(show, countField::kTotalUnits, 24, kToday);
  CHECK(withTotal.totalUnits == 24);
  CHECK(withTotal.progress == 10);
  CHECK(formatDate(withTotal.updatedAt) == "2026-10-20");
  // A total below the progress lowers the progress; 0 means unknown.
  CHECK(catalist::setCount(show, countField::kTotalUnits, 6, kToday).progress ==
        6);
  CHECK(catalist::setCount(withTotal, countField::kTotalUnits, 0, kToday)
            .progress == 10);
  CHECK(catalist::setCount(show, countField::kTotalUnits, -5, kToday)
            .totalUnits == 0);

  entry book = makeEntry().type(catalist::mediaType::kManga).totalVolumes(11);
  CHECK(catalist::setCount(book, countField::kVolumes, 4, kToday).volumes == 4);
  CHECK(catalist::setCount(book, countField::kVolumes, 40, kToday).volumes ==
        11);
  entry open = makeEntry().type(catalist::mediaType::kManga);
  CHECK(catalist::setCount(open, countField::kVolumes, 40, kToday).volumes ==
        40);

  CHECK(catalist::setCount(show, countField::kRewatched, 2, kToday).rewatched ==
        2);
  CHECK(
      catalist::setCount(show, countField::kRewatched, -1, kToday).rewatched ==
      0);
  CHECK_FALSE(catalist::setCount(show, countField::kRewatched, 0, kToday)
                  .updatedAt.has_value());
}

TEST_CASE("countOf reads each count") {
  entry e = makeEntry().progress(1).totalUnits(2).volumes(3).rewatched(4);
  CHECK(catalist::countOf(e, countField::kProgress) == 1);
  CHECK(catalist::countOf(e, countField::kTotalUnits) == 2);
  CHECK(catalist::countOf(e, countField::kVolumes) == 3);
  CHECK(catalist::countOf(e, countField::kRewatched) == 4);
}

TEST_CASE(
    "mergeImported refreshes totals and fills only what the user left empty") {
  entry mine =
      makeEntry().malId(5).score(8).progress(3).totalUnits(0).startDate(
          "2026-01-01");
  entry imported = makeEntry()
                       .malId(5)
                       .score(2)
                       .progress(9)
                       .totalUnits(12)
                       .totalVolumes(3)
                       .startDate("2020-01-01")
                       .finishDate("2020-02-02");
  auto merged = catalist::mergeImported(mine, imported);
  CHECK(merged.score == 8);
  CHECK(merged.progress == 3);
  CHECK(merged.totalUnits == 12);
  CHECK(merged.totalVolumes == 3);
  CHECK(formatDate(merged.startDate) == "2026-01-01");
  CHECK(formatDate(merged.finishDate) == "2020-02-02");
}
