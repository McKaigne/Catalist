#include "core/lists.hpp"

#include <algorithm>

#include "core/text.hpp"

namespace catalist {

namespace {

int SumOf(const std::vector<Entry>& list, int Entry::* count) {
	int total = 0;
	for (const auto& e : list)
		total += e.*count;
	return total;
}

std::string LastChange(const Entry& e) {
	return e.changed_at.empty() ? FormatDate(e.updated_at) : e.changed_at;
}

int StatusRank(WatchStatus status) {
	switch (status) {
		case WatchStatus::kWatching:
			return 0;
		case WatchStatus::kRewatching:
			return 1;
		case WatchStatus::kCompleted:
			return 2;
		case WatchStatus::kOnHold:
			return 3;
		case WatchStatus::kDropped:
			return 4;
		case WatchStatus::kPlanToWatch:
			return 5;
	}
	return 6;
}

// Negative when a goes first, positive when b does, 0 for a tie.
int Compare(const Entry& a, const Entry& b, SortKey key) {
	switch (key) {
		case SortKey::kUpdated:
			return LastChange(b).compare(LastChange(a));
		case SortKey::kStatus:
			return StatusRank(a.status) - StatusRank(b.status);
		case SortKey::kScore:
			return b.score - a.score;
		case SortKey::kProgress:
			return b.progress - a.progress;
		case SortKey::kStartDate:
			return FormatDate(b.start_date).compare(FormatDate(a.start_date));
		case SortKey::kTitle:
			break;
	}
	return 0;
}

}  // namespace

void SortEntries(std::vector<Entry>& entries, SortKey key) {
	std::stable_sort(entries.begin(), entries.end(), [key](const Entry& a, const Entry& b) {
		if (int order = Compare(a, b, key); order != 0)
			return order < 0;
		std::string name_a = utf8::Folded(DisplayName(a));
		std::string name_b = utf8::Folded(DisplayName(b));
		return name_a != name_b ? name_a < name_b : DisplayName(a) < DisplayName(b);
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
