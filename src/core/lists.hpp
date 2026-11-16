#ifndef CATALIST_CORE_LISTS_HPP_
#define CATALIST_CORE_LISTS_HPP_

#include <cstddef>
#include <vector>

#include "core/entry.hpp"

// What can be worked out from a list of titles: its order and its numbers.
// Pure: titles in, titles or numbers out.
namespace catalist {

enum class sortKey {
	kNew,  // most new releases first, then the latest change
	kUpdated,
	kScore,
	kStatus,
	kTitle,
	kProgress,
	kDate
};

// Each key has a natural order: new releases, last change, score, progress
// and date from high to low, title and status from first to last.
// `reverse` flips it. Titles that compare equal keep their order.
void sortEntries(std::vector<entry>& entries, sortKey key, bool reverse);

[[nodiscard]] size_t countByStatus(const std::vector<entry>& list, watchStatus status);
// Over rated titles only; 0 when nothing is rated.
[[nodiscard]] double averageScore(const std::vector<entry>& list);
[[nodiscard]] int totalProgress(const std::vector<entry>& list);
[[nodiscard]] int totalVolumes(const std::vector<entry>& list);
[[nodiscard]] int totalRewatched(const std::vector<entry>& list);
// An estimate: 24 minutes an episode, 5 minutes a chapter.
[[nodiscard]] double daysSpent(const std::vector<entry>& list);

}  // namespace catalist

#endif  // CATALIST_CORE_LISTS_HPP_
