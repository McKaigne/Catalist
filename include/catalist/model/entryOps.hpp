#ifndef CATALIST_MODEL_ENTRY_OPS_HPP_
#define CATALIST_MODEL_ENTRY_OPS_HPP_

#include "catalist/core/date.hpp"
#include "catalist/model/entry.hpp"

// Pure watchlist rules shared by the TUI and the CLI. Each takes the entry
// by value and returns the updated copy; `today` stamps updatedAt and any
// start or finish date the rule fills in.
namespace catalist {

// Sets progress, never below zero. Reaching the known total clamps to it and
// marks the entry completed.
[[nodiscard]] entry setProgress(entry target, int progress,
                                const isoDate& today);

// One interactive step: stepping forward starts the entry (start date, and
// resumes it from planned/on-hold/dropped) and finishing stamps the finish
// date; stepping back below the total reopens a completed entry.
[[nodiscard]] entry stepProgress(entry target, int delta, const isoDate& today);

// Moves to the next (direction > 0) or previous status in the cycle
// Watching -> Completed -> On-Hold -> Dropped -> Plan to Watch, keeping the
// start/finish dates and progress consistent with the new status.
[[nodiscard]] entry cycleStatus(entry target, int direction,
                                const isoDate& today);

// Changes the score by delta, kept within 0..10.
[[nodiscard]] entry adjustRating(entry target, int delta, const isoDate& today);

}  // namespace catalist

#endif  // CATALIST_MODEL_ENTRY_OPS_HPP_
