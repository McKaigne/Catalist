#ifndef CATALIST_MODEL_ACTIVITY_HPP_
#define CATALIST_MODEL_ACTIVITY_HPP_

#include <string>

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

}  // namespace catalist

#endif  // CATALIST_MODEL_ACTIVITY_HPP_
