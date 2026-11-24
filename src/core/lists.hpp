#ifndef CATALIST_CORE_LISTS_HPP_
#define CATALIST_CORE_LISTS_HPP_

#include <cstddef>
#include <vector>

#include "core/entry.hpp"

namespace catalist {

enum class SortKey { kUpdated, kScore, kStatus, kTitle, kProgress, kDate };

void SortEntries(std::vector<Entry>& entries, SortKey key, bool reverse);

[[nodiscard]] size_t CountByStatus(const std::vector<Entry>& list, WatchStatus status);
[[nodiscard]] double AverageScore(const std::vector<Entry>& list);
[[nodiscard]] int TotalProgress(const std::vector<Entry>& list);
[[nodiscard]] int TotalVolumes(const std::vector<Entry>& list);
[[nodiscard]] int TotalRewatched(const std::vector<Entry>& list);
[[nodiscard]] double DaysSpent(const std::vector<Entry>& list);

}  // namespace catalist

#endif  // CATALIST_CORE_LISTS_HPP_
