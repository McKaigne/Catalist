#ifndef CATALIST_ENTRY_HPP_
#define CATALIST_ENTRY_HPP_

#include <optional>
#include <string>

#include "date.hpp"

namespace catalist {

enum class mediaType { kAnime, kManga };

enum class watchStatus { kWatching, kCompleted, kOnHold, kDropped, kPlanToWatch };

struct entry {
	int id = 0;
	int malId = 0;
	std::string title;
	mediaType type = mediaType::kAnime;
	watchStatus status = watchStatus::kWatching;
	int score = 0;
	int progress = 0;
	int totalUnits = 0;
	int volumes = 0;
	int totalVolumes = 0;
	int rewatched = 0;
	std::optional<isoDate> startDate;
	std::optional<isoDate> finishDate;
	std::optional<isoDate> updatedAt;
	// Episodes or chapters out so far, from the last web check (0: unknown),
	// and when that check was.
	int released = 0;
	std::optional<isoDate> checkedAt;
};

std::string statusToString(watchStatus status, mediaType type = mediaType::kAnime);
// Accepts the display names and MyAnimeList's 1-5 codes, in any case;
// absent for anything else.
[[nodiscard]] std::optional<watchStatus> parseStatus(const std::string& str);

std::string typeToString(mediaType type);

// The display and file form of an optional date: ISO text, or empty.
[[nodiscard]] std::string formatDate(const std::optional<isoDate>& date);

// The one date the list shows for an entry: when it was finished if it is
// completed, else when it was started, else when it was last changed.
[[nodiscard]] std::optional<isoDate> listDate(const entry& shown);

// Released episodes or chapters not yet watched or read, for titles being
// followed (watching or on hold); 0 for everything else.
[[nodiscard]] int newReleases(const entry& shown);

}  // namespace catalist

// Pure watchlist rules. Each takes the entry by value and returns the updated
// copy; `today` stamps updatedAt and any start or finish date the rule fills
// in. A change that changes nothing returns the entry exactly as it was.
namespace catalist {

// Moves progress by delta, never below zero nor past a known total. Moving
// forward starts the entry (start date, and resumes it from planned, on-hold
// or dropped); reaching the total completes it with a finish date. Moving
// back below the total reopens a completed entry.
[[nodiscard]] entry stepProgress(entry target, int delta, const isoDate& today);

// Moves to the next (direction > 0) or previous status in the cycle
// Watching -> Completed -> On-Hold -> Dropped -> Plan to Watch, keeping the
// start/finish dates and progress consistent with the new status.
[[nodiscard]] entry cycleStatus(entry target, int direction, const isoDate& today);

// Changes the score by delta, kept within 0..10.
[[nodiscard]] entry adjustRating(entry target, int delta, const isoDate& today);

// Sets the status, filling in the start and finish dates the way the cycle
// does. Setting the status it already has changes nothing.
[[nodiscard]] entry setStatus(entry target, watchStatus status, const isoDate& today);

// Sets the score, kept within 0..10.
[[nodiscard]] entry setScore(entry target, int score, const isoDate& today);

// The numbers of an entry the user can set directly.
enum class countField { kProgress, kTotalUnits, kVolumes, kRewatched };

[[nodiscard]] int countOf(const entry& target, countField field);

// Sets one count, never below zero. Progress follows the rules of
// stepProgress; a total below the progress lowers the progress to it; volumes
// stay within a known volume total.
[[nodiscard]] entry setCount(entry target, countField field, int value, const isoDate& today);

// Folds an imported row into an entry the user already has. Totals and the
// MyAnimeList id are refreshed, but the user's own score, progress and dates
// win: imported values only fill fields the user never set.
[[nodiscard]] entry mergeImported(entry existing, const entry& incoming);

// What a web check found out about a title; 0 means it did not say.
struct webFacts {
	int totalUnits = 0;
	int totalVolumes = 0;
	int released = 0;
};

// Takes the totals and the released count from a web check, keeping the
// known values where the check had none, and stamps checkedAt. The user's
// own fields, updatedAt included, are left alone.
[[nodiscard]] entry applyWebFacts(entry target, const webFacts& facts, const isoDate& today);

}  // namespace catalist

#endif  // CATALIST_ENTRY_HPP_
