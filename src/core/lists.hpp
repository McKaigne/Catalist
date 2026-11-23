#ifndef CATALIST_CORE_LISTS_HPP_
#define CATALIST_CORE_LISTS_HPP_

#include <cstddef>
#include <vector>

#include "core/entry.hpp"

// What can be worked out from a list of titles: its order and its numbers.
// Pure: titles in, titles or numbers out.
namespace catalist {

enum class SortKey { kUpdated, kScore, kStatus, kTitle, kProgress, kDate };

// Each key has a natural order: last change, score, progress and date from
// high to low, title and status from first to last.
// `reverse` flips it. Titles that compare equal keep their order.
void SortEntries(std::vector<Entry>& entries, SortKey key, bool reverse);

[[nodiscard]] size_t CountByStatus(const std::vector<Entry>& list, WatchStatus status);
// Over rated titles only; 0 when nothing is rated.
[[nodiscard]] double AverageScore(const std::vector<Entry>& list);
[[nodiscard]] int TotalProgress(const std::vector<Entry>& list);
[[nodiscard]] int TotalVolumes(const std::vector<Entry>& list);
[[nodiscard]] int TotalRewatched(const std::vector<Entry>& list);
// An estimate: 24 minutes an episode, 5 minutes a chapter.
[[nodiscard]] double DaysSpent(const std::vector<Entry>& list);

}  // namespace catalist

#endif  // CATALIST_CORE_LISTS_HPP_
