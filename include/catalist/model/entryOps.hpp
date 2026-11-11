#ifndef CATALIST_MODEL_ENTRY_OPS_HPP_
#define CATALIST_MODEL_ENTRY_OPS_HPP_

#include "catalist/core/date.hpp"
#include "catalist/model/entry.hpp"

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
[[nodiscard]] entry cycleStatus(entry target, int direction,
                                const isoDate& today);

// Changes the score by delta, kept within 0..10.
[[nodiscard]] entry adjustRating(entry target, int delta, const isoDate& today);

// The numbers of an entry the user can set directly.
enum class countField { kProgress, kTotalUnits, kVolumes, kRewatched };

[[nodiscard]] int countOf(const entry& target, countField field);

// Sets one count, never below zero. Progress follows the rules of
// stepProgress; a total below the progress lowers the progress to it; volumes
// stay within a known volume total.
[[nodiscard]] entry setCount(entry target, countField field, int value,
                             const isoDate& today);

// Folds an imported row into an entry the user already has. Totals and the
// MyAnimeList id are refreshed, but the user's own score, progress and dates
// win: imported values only fill fields the user never set.
[[nodiscard]] entry mergeImported(entry existing, const entry& incoming);

}  // namespace catalist

#endif  // CATALIST_MODEL_ENTRY_OPS_HPP_
