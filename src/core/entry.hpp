#ifndef CATALIST_CORE_ENTRY_HPP_
#define CATALIST_CORE_ENTRY_HPP_

#include <optional>
#include <string>

#include "core/date.hpp"

namespace catalist {

enum class MediaType { kAnime, kManga };

// kRewatching is watching (or reading) a completed title again, as AniList's
// REPEATING and MyAnimeList's is_rewatching have it.
enum class WatchStatus { kWatching, kCompleted, kOnHold, kDropped, kPlanToWatch, kRewatching };

struct Entry {
	// Numbers the title within its list: the 12 of the ref a:12.
	int id = 0;
	// The title on each site; 0 while not known. AniList knows its own and
	// MyAnimeList's, so a lookup fills in whichever is missing.
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
	std::optional<IsoDate> start_date;
	std::optional<IsoDate> finish_date;
	std::optional<IsoDate> updated_at;
	std::string changed_at;

	bool operator==(const Entry&) const = default;
};

std::string StatusToString(WatchStatus status, MediaType type = MediaType::kAnime);
// The one word a status is stored and printed for scripts as: watching,
// completed, on-hold, dropped, planned or rewatching. ParseStatus reads it
// back.
[[nodiscard]] std::string StatusKey(WatchStatus status);
// Accepts the display names and MyAnimeList's 1-5 codes, in any case
// (rewatching, rereading and repeating for kRewatching); absent for anything
// else.
[[nodiscard]] std::optional<WatchStatus> ParseStatus(const std::string& str);

std::string TypeToString(MediaType type);
// "anime" or "manga".
[[nodiscard]] std::string TypeKey(MediaType type);

[[nodiscard]] const std::string& DisplayName(const Entry& shown);

// The title's page on AniList; empty without an AniList id.
[[nodiscard]] std::string PageOf(const Entry& shown);

// The display and file form of an optional date: ISO text, or empty.
[[nodiscard]] std::string FormatDate(const std::optional<IsoDate>& date);

// The one date the list shows for an entry: when it was finished if it is
// completed, else when it was started, else when it was last changed.
[[nodiscard]] std::optional<IsoDate> ListDate(const Entry& shown);

}  // namespace catalist

// Pure watchlist rules. Each takes the entry by value and returns the updated
// copy; `today` stamps updated_at and any start or finish date the rule fills
// in. A change that changes nothing returns the entry exactly as it was.
namespace catalist {

[[nodiscard]] bool AtTotal(const Entry& target);

// Why a title cannot take this status: resuming a completed title, or
// rewatching one that was never completed. Nothing when it can.
[[nodiscard]] std::optional<std::string> StatusRefusal(const Entry& target, WatchStatus status);

// Moves progress by delta, never below zero nor past a known total. Moving
// forward starts the entry (start date, and resumes it from planned, on-hold
// or dropped); reaching the total completes it with a finish date, and ends a
// rewatch with one more to the rewatch count. Moving back below the total
// reopens a completed entry. A finished rewatch keeps the first finish date.
[[nodiscard]] Entry StepProgress(Entry target, int delta, const IsoDate& today);

// Sets the status, keeping the start and finish dates and the progress
// consistent with it. A rewatch starts again from the first episode or
// chapter, and keeps the dates of the first time; holding, dropping or
// planning it instead returns it to completed. Only a completed or rewatching
// entry keeps a finish date. Setting the status it already has changes nothing.
[[nodiscard]] Entry SetStatus(Entry target, WatchStatus status, const IsoDate& today);

// Sets the score, kept within 0..10.
[[nodiscard]] Entry SetScore(Entry target, int score, const IsoDate& today);

// The numbers of an entry the user can set directly.
enum class CountField { kProgress, kTotalUnits, kVolumes, kRewatched };

[[nodiscard]] int CountOf(const Entry& target, CountField field);

// Sets one count, never below zero. Progress follows the rules of
// StepProgress; a total below the progress lowers the progress to it; volumes
// stay within a known volume total.
[[nodiscard]] Entry SetCount(Entry target, CountField field, int value, const IsoDate& today);

[[nodiscard]] Entry SetTitle(Entry target, const std::string& title, const IsoDate& today);

// Sets the day the entry was started or finished, or clears it. The status
// is left as it is: these correct a date, they do not start or finish.
[[nodiscard]] Entry SetStartDate(Entry target, std::optional<IsoDate> day, const IsoDate& today);
[[nodiscard]] Entry SetFinishDate(Entry target, std::optional<IsoDate> day, const IsoDate& today);

// Folds an imported row into an entry the user already has. Totals and the
// ids are refreshed, but the user's own score, progress and dates
// win: imported values only fill fields the user never set.
[[nodiscard]] Entry MergeImported(Entry existing, const Entry& incoming);

}  // namespace catalist

#endif  // CATALIST_CORE_ENTRY_HPP_
