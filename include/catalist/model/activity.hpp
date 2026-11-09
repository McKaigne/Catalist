#ifndef CATALIST_MODEL_ACTIVITY_HPP_
#define CATALIST_MODEL_ACTIVITY_HPP_

#include <optional>
#include <string>
#include <vector>

#include "catalist/core/clock.hpp"
#include "catalist/model/entry.hpp"

namespace catalist {

struct activityRecord {
  std::string timestamp;
  mediaType type = mediaType::kAnime;
  std::string title;
  watchStatus status = watchStatus::kWatching;
  int progress = 0;
  int totalUnits = 0;
  int score = 0;
};

// History timestamps are "YYYY-MM-DD HH:MM". Files written before 0.3.0 used
// "Mon DD, HH:MM AM" without a year; those resolve to the most recent such
// day that is not after `today`.
[[nodiscard]] std::optional<isoDate> activityDay(const activityRecord& record,
                                                 const isoDate& today);

[[nodiscard]] std::string activityTimestamp(const iClock& clock);

struct historyDay {
  isoDate day;
  std::vector<activityRecord> records;
};

// Groups history records by calendar day, newest day first. Records whose
// timestamp cannot be dated are left out.
[[nodiscard]] std::vector<historyDay> groupHistoryByDay(
    const std::vector<activityRecord>& records, const isoDate& today);

}  // namespace catalist

#endif  // CATALIST_MODEL_ACTIVITY_HPP_
