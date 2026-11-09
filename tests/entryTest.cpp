#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "catalist/entry.hpp"

using catalist::watchStatus;
using catalist::stringToStatus;

namespace {

void checkAllMapTo(const std::vector<std::string>& spellings,
                   watchStatus expected) {
  for (const auto& spelling : spellings) {
    CAPTURE(spelling);
    CHECK(stringToStatus(spelling) == expected);
  }
}

}  // namespace

TEST_CASE("stringToStatus watching spellings") {
  checkAllMapTo({"watching", "Watching", "WATCHING", "watch", "reading",
                 "Reading", "read", "rewatching", "1"},
                watchStatus::kWatching);
}

TEST_CASE("stringToStatus completed spellings") {
  checkAllMapTo({"completed", "Completed", "complete", "2"},
                watchStatus::kCompleted);
}

TEST_CASE("stringToStatus on hold spellings") {
  checkAllMapTo({"on-hold", "On-Hold", "onhold", "on hold", "hold", "3"},
                watchStatus::kOnHold);
}

TEST_CASE("stringToStatus dropped spellings") {
  checkAllMapTo({"dropped", "Dropped", "drop", "4"}, watchStatus::kDropped);
}

TEST_CASE("stringToStatus plan spellings") {
  checkAllMapTo({"plan to watch", "Plan to Watch", "Plan to Read", "planned",
                 "plan", "plan-to-watch", "plan-to-read", "5"},
                watchStatus::kPlanToWatch);
}

// Characterization: unknown input silently falls back to Plan to Watch.
TEST_CASE("stringToStatus unknown input falls back to plan to watch") {
  checkAllMapTo({"", "xyz", "0", "6", "ptw"}, watchStatus::kPlanToWatch);
}

TEST_CASE("statusToString round trips through stringToStatus") {
  for (auto type : {catalist::mediaType::kAnime, catalist::mediaType::kManga}) {
    for (auto status : {watchStatus::kWatching, watchStatus::kCompleted,
                        watchStatus::kOnHold, watchStatus::kDropped,
                        watchStatus::kPlanToWatch}) {
      CHECK(stringToStatus(catalist::statusToString(status, type)) == status);
    }
  }
}
