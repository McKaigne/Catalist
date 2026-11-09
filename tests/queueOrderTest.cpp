#include <doctest/doctest.h>

#include "catalist/model/queueOrder.hpp"
#include "support/testSupport.hpp"

using catalist::entry;
using catalist::watchStatus;
using catalist::test::makeEntry;

namespace {

entry queued(int id, const std::string& title, watchStatus status,
             std::optional<int> position = std::nullopt) {
  entry e = makeEntry().id(id).title(title).status(status);
  e.queuePos = position;
  return e;
}

std::vector<int> ids(const std::vector<entry>& entries) {
  std::vector<int> listed;
  for (const auto& e : entries)
    listed.push_back(e.id);
  return listed;
}

}  // namespace

TEST_CASE("orderedQueue keeps only watching and planned entries") {
  auto order = catalist::orderedQueue(
      {queued(1, "A", watchStatus::kCompleted), queued(2, "B", watchStatus::kWatching),
       queued(3, "C", watchStatus::kDropped), queued(4, "D", watchStatus::kPlanToWatch),
       queued(5, "E", watchStatus::kOnHold)});
  CHECK(ids(order) == std::vector<int>{2, 4});
}

TEST_CASE("orderedQueue puts explicit positions first then watching then title") {
  auto order = catalist::orderedQueue(
      {queued(1, "Zeta", watchStatus::kWatching),
       queued(2, "Alpha", watchStatus::kPlanToWatch),
       queued(3, "Beta", watchStatus::kWatching),
       queued(4, "Late", watchStatus::kPlanToWatch, 1),
       queued(5, "Early", watchStatus::kWatching, 0)});
  CHECK(ids(order) == std::vector<int>{5, 4, 3, 1, 2});
}

TEST_CASE("moveInQueue swaps and renumbers positions without gaps") {
  std::vector<entry> queue = {queued(1, "A", watchStatus::kWatching, 3),
                              queued(2, "B", watchStatus::kWatching),
                              queued(3, "C", watchStatus::kWatching, 9)};
  auto changed = catalist::moveInQueue(queue, 0, 1);
  CHECK(ids(queue) == std::vector<int>{2, 1, 3});
  CHECK(queue[0].queuePos == 0);
  CHECK(queue[1].queuePos == 1);
  CHECK(queue[2].queuePos == 2);
  CHECK(changed.size() == 3);
}

TEST_CASE("moveInQueue ignores moves outside the queue") {
  std::vector<entry> queue = {queued(1, "A", watchStatus::kWatching)};
  CHECK(catalist::moveInQueue(queue, 0, 1).empty());
  CHECK_FALSE(queue[0].queuePos.has_value());
}
