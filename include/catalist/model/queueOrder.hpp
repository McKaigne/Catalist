#ifndef CATALIST_MODEL_QUEUE_ORDER_HPP_
#define CATALIST_MODEL_QUEUE_ORDER_HPP_

#include <cstddef>
#include <vector>

#include "catalist/model/entry.hpp"

namespace catalist {

[[nodiscard]] bool isQueued(const entry& candidate);

// The Watching and Plan to Watch entries among `entries`, in queue order:
// explicit positions first (ascending), then Watching before Plan to Watch,
// then title.
[[nodiscard]] std::vector<entry> orderedQueue(const std::vector<entry>& entries);

// Swaps queue[from] with queue[to] and renumbers queuePos 0..n-1 so the
// stored order never has gaps or ties. Returns the entries whose position
// changed.
[[nodiscard]] std::vector<entry> moveInQueue(std::vector<entry>& queue,
                                             size_t from, size_t to);

}  // namespace catalist

#endif  // CATALIST_MODEL_QUEUE_ORDER_HPP_
