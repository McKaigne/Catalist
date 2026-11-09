#include "catalist/model/queueOrder.hpp"

#include <algorithm>
#include <iterator>

namespace catalist {

namespace {

bool comesFirst(const entry& left, const entry& right) {
  if (left.queuePos.has_value() != right.queuePos.has_value())
    return left.queuePos.has_value();
  if (left.queuePos && *left.queuePos != *right.queuePos)
    return *left.queuePos < *right.queuePos;
  if (left.status != right.status)
    return left.status == watchStatus::kWatching;
  return left.title < right.title;
}

}  // namespace

bool isQueued(const entry& candidate) {
  return candidate.status == watchStatus::kWatching ||
         candidate.status == watchStatus::kPlanToWatch;
}

std::vector<entry> orderedQueue(const std::vector<entry>& entries) {
  std::vector<entry> queued;
  std::copy_if(entries.begin(), entries.end(), std::back_inserter(queued),
               isQueued);
  std::stable_sort(queued.begin(), queued.end(), comesFirst);
  return queued;
}

std::vector<entry> moveInQueue(std::vector<entry>& queue, size_t from,
                               size_t to) {
  if (from >= queue.size() || to >= queue.size())
    return {};
  std::swap(queue[from], queue[to]);
  std::vector<entry> changed;
  for (size_t position = 0; position < queue.size(); ++position) {
    auto& queued = queue[position];
    if (queued.queuePos == static_cast<int>(position))
      continue;
    queued.queuePos = static_cast<int>(position);
    changed.push_back(queued);
  }
  return changed;
}

}  // namespace catalist
