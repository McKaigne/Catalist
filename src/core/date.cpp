#include "catalist/core/date.hpp"

#include <iomanip>
#include <sstream>

namespace catalist {

std::optional<isoDate> isoDate::parse(std::string_view dateStr) {
  if (dateStr.size() < 10)
    return std::nullopt;
  isoDate date;
  try {
    date.year_ = std::stoi(std::string(dateStr.substr(0, 4)));
    date.month_ = std::stoi(std::string(dateStr.substr(5, 2)));
    date.day_ = std::stoi(std::string(dateStr.substr(8, 2)));
  } catch (...) {
    return std::nullopt;
  }
  if (date.year_ <= 0)
    return std::nullopt;
  return date;
}

isoDate isoDate::fromLocalTm(const std::tm& local) {
  isoDate date;
  date.year_ = local.tm_year + 1900;
  date.month_ = local.tm_mon + 1;
  date.day_ = local.tm_mday;
  return date;
}

// Howard Hinnant's days_from_civil / civil_from_days: exact for the whole
// Gregorian calendar without going through time_t or the C locale.
long isoDate::dayNumber() const {
  long year = year_ - (month_ <= 2 ? 1 : 0);
  long era = (year >= 0 ? year : year - 399) / 400;
  long yearOfEra = year - era * 400;
  long shiftedMonth = month_ + (month_ > 2 ? -3 : 9);
  long dayOfYear = (153 * shiftedMonth + 2) / 5 + day_ - 1;
  long dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
  return era * 146097 + dayOfEra - 719468;
}

isoDate isoDate::fromDayNumber(long dayNumber) {
  long shifted = dayNumber + 719468;
  long era = (shifted >= 0 ? shifted : shifted - 146096) / 146097;
  long dayOfEra = shifted - era * 146097;
  long yearOfEra =
      (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 - dayOfEra / 146096) / 365;
  long dayOfYear = dayOfEra - (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
  long shiftedMonth = (5 * dayOfYear + 2) / 153;
  isoDate date;
  date.day_ = static_cast<int>(dayOfYear - (153 * shiftedMonth + 2) / 5 + 1);
  date.month_ = static_cast<int>(shiftedMonth < 10 ? shiftedMonth + 3
                                                   : shiftedMonth - 9);
  date.year_ = static_cast<int>(yearOfEra + era * 400 + (date.month_ <= 2 ? 1 : 0));
  return date;
}

int isoDate::weekday() const {
  // 1970-01-01 was a Thursday (3 when Monday is 0).
  return static_cast<int>(((dayNumber() % 7) + 7 + 3) % 7);
}

isoDate isoDate::addDays(long days) const {
  return fromDayNumber(dayNumber() + days);
}

std::string isoDate::toString() const {
  std::ostringstream ss;
  ss << std::setfill(char(48)) << std::setw(4) << year_ << "-" << std::setw(2)
     << month_ << "-" << std::setw(2) << day_;
  return ss.str();
}

}  // namespace catalist
