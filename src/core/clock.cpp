#include "catalist/core/clock.hpp"

#include <ctime>

namespace catalist {

iClock::timePoint realClock::now() const {
  return std::chrono::system_clock::now();
}

isoDate localDate(const iClock& clock) {
  std::time_t seconds = std::chrono::system_clock::to_time_t(clock.now());
  std::tm local{};
  localtime_r(&seconds, &local);
  return isoDate::fromLocalTm(local);
}

}  // namespace catalist
