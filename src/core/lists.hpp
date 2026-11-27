#ifndef CATALIST_CORE_LISTS_HPP_
#define CATALIST_CORE_LISTS_HPP_

#include <cstddef>
#include <vector>

#include "core/entry.hpp"

namespace catalist {

enum class SortKey { kUpdated, kTitle, kStatus, kScore, kProgress, kStartDate };

// The latest change, the highest score, the most progress and the latest start
// come first; titles A to Z; statuses in progress first, planned last. Ties
// go by name.
void SortEntries(std::vector<Entry>& entries, SortKey key);

[[nodiscard]] size_t CountByStatus(const std::vector<Entry>& list, WatchStatus status);
[[nodiscard]] double AverageScore(const std::vector<Entry>& list);
[[nodiscard]] int TotalProgress(const std::vector<Entry>& list);
[[nodiscard]] int TotalVolumes(const std::vector<Entry>& list);
[[nodiscard]] int TotalRewatched(const std::vector<Entry>& list);
[[nodiscard]] double DaysSpent(const std::vector<Entry>& list);

}  // namespace catalist

#endif  // CATALIST_CORE_LISTS_HPP_
