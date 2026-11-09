#include "catalist/model/activity.hpp"

#include <array>
#include <cstdio>

namespace catalist {

namespace {

std::optional<int> monthFromAbbreviation(const std::string& text) {
  static const std::array<const char*, 12> kMonths = {
      "Jan", "Feb", "Mar", "Apr", "May", "Jun",
      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  for (size_t i = 0; i < kMonths.size(); ++i) {
    if (text.rfind(kMonths[i], 0) == 0)
      return static_cast<int>(i) + 1;
  }
  return std::nullopt;
}

std::optional<isoDate> legacyDay(const std::string& timestamp,
                                 const isoDate& today) {
  auto month = monthFromAbbreviation(timestamp);
  if (!month || timestamp.size() < 6)
    return std::nullopt;
  int day = 0;
  try {
    day = std::stoi(timestamp.substr(4, 2));
  } catch (...) {
    return std::nullopt;
  }
  char buffer[11];
  std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", today.year(), *month,
                day);
  auto candidate = isoDate::parse(buffer);
  if (candidate && *candidate > today) {
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", today.year() - 1,
                  *month, day);
    candidate = isoDate::parse(buffer);
  }
  return candidate;
}

}  // namespace

std::optional<isoDate> activityDay(const activityRecord& record,
                                   const isoDate& today) {
  if (auto isoDay = isoDate::parse(record.timestamp.substr(0, 10)))
    return isoDay;
  return legacyDay(record.timestamp, today);
}

std::string activityTimestamp(const iClock& clock) {
  return formatLocalTime(clock, "%Y-%m-%d %H:%M");
}

}  // namespace catalist
