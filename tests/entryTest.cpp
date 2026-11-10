#include <doctest/doctest.h>

#include <string>
#include <vector>

#include <unordered_map>

#include "catalist/model/entry.hpp"

using catalist::watchStatus;
using catalist::parseStatus;

namespace {

void checkAllMapTo(const std::vector<std::string>& spellings,
                   watchStatus expected) {
  for (const auto& spelling : spellings) {
    CAPTURE(spelling);
    CHECK(parseStatus(spelling) == expected);
  }
}

}  // namespace

TEST_CASE("parseStatus watching spellings") {
  checkAllMapTo({"watching", "Watching", "WATCHING", "watch", "reading",
                 "Reading", "read", "rewatching", "1"},
                watchStatus::kWatching);
}

TEST_CASE("parseStatus completed spellings") {
  checkAllMapTo({"completed", "Completed", "complete", "2"},
                watchStatus::kCompleted);
}

TEST_CASE("parseStatus on hold spellings") {
  checkAllMapTo({"on-hold", "On-Hold", "onhold", "on hold", "hold", "3"},
                watchStatus::kOnHold);
}

TEST_CASE("parseStatus dropped spellings") {
  checkAllMapTo({"dropped", "Dropped", "drop", "4"}, watchStatus::kDropped);
}

TEST_CASE("parseStatus plan spellings") {
  checkAllMapTo({"plan to watch", "Plan to Watch", "Plan to Read", "planned",
                 "plan", "plan-to-watch", "plan-to-read", "5"},
                watchStatus::kPlanToWatch);
}

TEST_CASE("parseStatus rejects unknown input") {
  for (const char* spelling : {"", "xyz", "0", "6", "ptw"}) {
    CAPTURE(spelling);
    CHECK_FALSE(parseStatus(spelling).has_value());
  }
}

TEST_CASE("statusToString round trips through parseStatus") {
  for (auto type : {catalist::mediaType::kAnime, catalist::mediaType::kManga}) {
    for (auto status : {watchStatus::kWatching, watchStatus::kCompleted,
                        watchStatus::kOnHold, watchStatus::kDropped,
                        watchStatus::kPlanToWatch}) {
      CHECK(parseStatus(catalist::statusToString(status, type)) == status);
    }
  }
}
