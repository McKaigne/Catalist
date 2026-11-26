#ifndef CATALIST_CORE_ENTRY_HPP_
#define CATALIST_CORE_ENTRY_HPP_

#include <optional>
#include <string>

#include "core/date.hpp"

namespace catalist {

enum class MediaType { kAnime, kManga };

enum class WatchStatus { kWatching, kCompleted, kOnHold, kDropped, kPlanToWatch, kRewatching };

// MyAnimeList's rewatch (reread) value, 1 to 5 in its XML.
enum class RewatchValue { kNone, kVeryLow, kLow, kMedium, kHigh, kVeryHigh };

struct Entry {
	int id = 0;
	int anilist_id = 0;
	int mal_id = 0;
	std::string title;
	std::string english_title;
	MediaType type = MediaType::kAnime;
	WatchStatus status = WatchStatus::kWatching;
	int score = 0;
	int progress = 0;
	int total_units = 0;
	int volumes = 0;
	int total_volumes = 0;
	int rewatched = 0;
	RewatchValue rewatch_value = RewatchValue::kNone;
	std::optional<IsoDate> start_date;
	std::optional<IsoDate> finish_date;
	std::optional<IsoDate> updated_at;
	std::string changed_at;

	bool operator==(const Entry&) const = default;
};

std::string StatusToString(WatchStatus status, MediaType type = MediaType::kAnime);
[[nodiscard]] std::string StatusKey(WatchStatus status);
[[nodiscard]] std::optional<WatchStatus> ParseStatus(const std::string& str);

[[nodiscard]] std::string RewatchValueLabel(RewatchValue value);
[[nodiscard]] std::string RewatchValueKey(RewatchValue value);
[[nodiscard]] std::optional<RewatchValue> ParseRewatchValue(const std::string& key);

std::string TypeToString(MediaType type);
[[nodiscard]] std::string TypeKey(MediaType type);

[[nodiscard]] const std::string& DisplayName(const Entry& shown);

[[nodiscard]] std::string PageOf(const Entry& shown);

[[nodiscard]] std::string FormatDate(const std::optional<IsoDate>& date);

}  // namespace catalist

namespace catalist {

[[nodiscard]] bool AtTotal(const Entry& target);

[[nodiscard]] std::optional<std::string> StatusRefusal(const Entry& target, WatchStatus status);

[[nodiscard]] Entry StepProgress(Entry target, int delta, const IsoDate& today);

[[nodiscard]] Entry SetStatus(Entry target, WatchStatus status, const IsoDate& today);

[[nodiscard]] Entry SetScore(Entry target, int score, const IsoDate& today);

[[nodiscard]] Entry SetRewatchValue(Entry target, RewatchValue value, const IsoDate& today);

enum class CountField { kProgress, kTotalUnits, kVolumes };

[[nodiscard]] int CountOf(const Entry& target, CountField field);

[[nodiscard]] Entry SetCount(Entry target, CountField field, int value, const IsoDate& today);

[[nodiscard]] Entry SetTitle(Entry target, const std::string& title, const IsoDate& today);

[[nodiscard]] Entry SetStartDate(Entry target, std::optional<IsoDate> day, const IsoDate& today);
[[nodiscard]] Entry SetFinishDate(Entry target, std::optional<IsoDate> day, const IsoDate& today);

[[nodiscard]] Entry MergeImported(Entry existing, const Entry& incoming);

[[nodiscard]] Entry TakeImported(Entry existing, const Entry& incoming);

}  // namespace catalist

#endif  // CATALIST_CORE_ENTRY_HPP_
