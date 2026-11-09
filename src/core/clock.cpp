#include "catalist/core/clock.hpp"

#include <ctime>

namespace catalist {

namespace {

std::tm toLocalTm(iClock::timePoint moment) {
  std::time_t seconds = std::chrono::system_clock::to_time_t(moment);
  std::tm local{};
  localtime_r(&seconds, &local);
  return local;
}

}  // namespace

iClock::timePoint realClock::now() const {
  return std::chrono::system_clock::now();
}

isoDate localDate(const iClock& clock) {
  return isoDate::fromLocalTm(toLocalTm(clock.now()));
}

std::string formatLocalTime(const iClock& clock, const char* pattern) {
  std::tm local = toLocalTm(clock.now());
  char buffer[64];
  size_t written = std::strftime(buffer, sizeof(buffer), pattern, &local);
  return std::string(buffer, written);
}

}  // namespace catalist
