#include "core/lists.hpp"

#include <algorithm>

namespace catalist {

namespace {

// The sum of one count over a list, e.g. sumOf(list, &entry::progress).
int sumOf(const std::vector<entry>& list, int entry::* count) {
	int total = 0;
	for (const auto& e : list)
		total += e.*count;
	return total;
}

// True when `a` comes before `b` in the natural order of `key`; ties fall
// back to the title so the order never depends on how the list was stored.
bool comesBefore(const entry& a, const entry& b, sortKey key) {
	switch (key) {
		case sortKey::kNew:
			if (newReleases(a) != newReleases(b))
				return newReleases(a) > newReleases(b);
			[[fallthrough]];
		case sortKey::kUpdated:
			if (a.updatedAt != b.updatedAt)
				return a.updatedAt > b.updatedAt;
			break;
		case sortKey::kScore:
			if (a.score != b.score)
				return a.score > b.score;
			break;
		case sortKey::kProgress:
			if (a.progress != b.progress)
				return a.progress > b.progress;
			break;
		case sortKey::kDate:
			if (listDate(a) != listDate(b))
				return listDate(a) > listDate(b);
			break;
		case sortKey::kStatus:
			if (a.status != b.status)
				return static_cast<int>(a.status) < static_cast<int>(b.status);
			break;
		case sortKey::kTitle:
			break;
	}
	return a.title < b.title;
}

}  // namespace

void sortEntries(std::vector<entry>& entries, sortKey key, bool reverse) {
	// Stable, so titles that compare equal keep the order they came in and
	// the same command prints the same lines every time.
	std::stable_sort(entries.begin(), entries.end(),
	                 [key, reverse](const entry& a, const entry& b) {
		                 return reverse ? comesBefore(b, a, key) : comesBefore(a, b, key);
	                 });
}

size_t countByStatus(const std::vector<entry>& list, watchStatus status) {
	return static_cast<size_t>(std::count_if(
	    list.begin(), list.end(), [status](const entry& e) { return e.status == status; }));
}

double averageScore(const std::vector<entry>& list) {
	int sum = 0;
	int rated = 0;
	for (const auto& e : list) {
		if (e.score > 0) {
			sum += e.score;
			++rated;
		}
	}
	return rated == 0 ? 0.0 : static_cast<double>(sum) / rated;
}

int totalProgress(const std::vector<entry>& list) {
	return sumOf(list, &entry::progress);
}

int totalVolumes(const std::vector<entry>& list) {
	return sumOf(list, &entry::volumes);
}

int totalRewatched(const std::vector<entry>& list) {
	return sumOf(list, &entry::rewatched);
}

double daysSpent(const std::vector<entry>& list) {
	constexpr double kMinutesPerDay = 1440.0;
	double minutes = 0.0;
	for (const auto& e : list)
		minutes += e.progress * (e.type == mediaType::kAnime ? 24.0 : 5.0);
	return minutes / kMinutesPerDay;
}

}  // namespace catalist
