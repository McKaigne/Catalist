#include "core/lists.hpp"

#include <algorithm>

namespace catalist {

namespace {

// The sum of one count over a list, e.g. SumOf(list, &entry::progress).
int SumOf(const std::vector<Entry>& list, int Entry::* count) {
	int total = 0;
	for (const auto& e : list)
		total += e.*count;
	return total;
}

std::string LastChange(const Entry& e) {
	return e.changed_at.empty() ? FormatDate(e.updated_at) : e.changed_at;
}

// True when `a` comes before `b` in the natural order of `key`; ties fall
// back to the title so the order never depends on how the list was stored.
bool ComesBefore(const Entry& a, const Entry& b, SortKey key) {
	switch (key) {
		case SortKey::kUpdated:
			if (LastChange(a) != LastChange(b))
				return LastChange(a) > LastChange(b);
			break;
		case SortKey::kScore:
			if (a.score != b.score)
				return a.score > b.score;
			break;
		case SortKey::kProgress:
			if (a.progress != b.progress)
				return a.progress > b.progress;
			break;
		case SortKey::kDate:
			if (ListDate(a) != ListDate(b))
				return ListDate(a) > ListDate(b);
			break;
		case SortKey::kStatus:
			if (a.status != b.status)
				return static_cast<int>(a.status) < static_cast<int>(b.status);
			break;
		case SortKey::kTitle:
			break;
	}
	return DisplayName(a) < DisplayName(b);
}

}  // namespace

void SortEntries(std::vector<Entry>& entries, SortKey key, bool reverse) {
	// Stable, so titles that compare equal keep the order they came in and
	// the same command prints the same lines every time.
	std::stable_sort(entries.begin(), entries.end(),
	                 [key, reverse](const Entry& a, const Entry& b) {
		                 return reverse ? ComesBefore(b, a, key) : ComesBefore(a, b, key);
	                 });
}

size_t CountByStatus(const std::vector<Entry>& list, WatchStatus status) {
	return static_cast<size_t>(std::count_if(
	    list.begin(), list.end(), [status](const Entry& e) { return e.status == status; }));
}

double AverageScore(const std::vector<Entry>& list) {
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

int TotalProgress(const std::vector<Entry>& list) {
	return SumOf(list, &Entry::progress);
}

int TotalVolumes(const std::vector<Entry>& list) {
	return SumOf(list, &Entry::volumes);
}

int TotalRewatched(const std::vector<Entry>& list) {
	return SumOf(list, &Entry::rewatched);
}

double DaysSpent(const std::vector<Entry>& list) {
	constexpr double kMinutesPerDay = 1440.0;
	double minutes = 0.0;
	for (const auto& e : list)
		minutes += e.progress * (e.type == MediaType::kAnime ? 24.0 : 5.0);
	return minutes / kMinutesPerDay;
}

}  // namespace catalist
