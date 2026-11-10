#include "catalist/core/date.hpp"

#include <charconv>
#include <iomanip>
#include <sstream>

namespace catalist {

namespace {

std::optional<int> parseDigits(std::string_view digits) {
  int number = 0;
  auto [end, error] =
      std::from_chars(digits.data(), digits.data() + digits.size(), number);
  if (error != std::errc() || end != digits.data() + digits.size() ||
      digits.front() == '-' || digits.front() == '+')
    return std::nullopt;
  return number;
}

bool isLeapYear(int year) {
  return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

int daysInMonth(int year, int month) {
  static const int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return month == 2 && isLeapYear(year) ? 29 : kDays[month - 1];
}

bool hasDateShape(std::string_view text) {
  if (text.size() < 10 || text[4] != '-' || text[7] != '-')
    return false;
  return text.size() == 10 || text[10] == 'T' || text[10] == ' ';
}

}  // namespace

std::optional<isoDate> isoDate::parse(std::string_view dateStr) {
  if (!hasDateShape(dateStr))
    return std::nullopt;
  auto year = parseDigits(dateStr.substr(0, 4));
  auto month = parseDigits(dateStr.substr(5, 2));
  auto day = parseDigits(dateStr.substr(8, 2));
  if (!year || !month || !day)
    return std::nullopt;
  return fromParts(*year, *month, *day);
}

std::optional<isoDate> isoDate::fromParts(int year, int month, int day) {
  if (year <= 0 || month < 1 || month > 12 || day < 1 ||
      day > daysInMonth(year, month))
    return std::nullopt;
  isoDate date;
  date.year_ = year;
  date.month_ = month;
  date.day_ = day;
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

std::string isoDate::toString() const {
  std::ostringstream ss;
  ss << std::setfill(char(48)) << std::setw(4) << year_ << "-" << std::setw(2)
     << month_ << "-" << std::setw(2) << day_;
  return ss.str();
}

}  // namespace catalist
