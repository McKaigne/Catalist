#ifndef CATALIST_MODEL_ACTIVITY_HPP_
#define CATALIST_MODEL_ACTIVITY_HPP_

#include <string>

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

// History timestamps are "YYYY-MM-DD HH:MM".
[[nodiscard]] std::string activityTimestamp(const iClock& clock);

}  // namespace catalist

#endif  // CATALIST_MODEL_ACTIVITY_HPP_
