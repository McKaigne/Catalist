#include "catalist/model/activity.hpp"

namespace catalist {

std::string activityTimestamp(const iClock& clock) {
  return formatLocalTime(clock, "%Y-%m-%d %H:%M");
}

}  // namespace catalist
