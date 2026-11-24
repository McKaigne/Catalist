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

}  // namespace

void SortEntries(std::vector<Entry>& entries, SortKey key) {
	std::stable_sort(entries.begin(), entries.end(), [key](const Entry& a, const Entry& b) {
		if (key == SortKey::kUpdated and LastChange(a) != LastChange(b))
			return LastChange(a) > LastChange(b);
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
