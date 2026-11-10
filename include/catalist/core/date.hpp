#ifndef CATALIST_CORE_DATE_HPP_
#define CATALIST_CORE_DATE_HPP_

#include <ctime>
#include <optional>
#include <string>
#include <string_view>

namespace catalist {

class isoDate {
public:
  // Accepts "YYYY-MM-DD", optionally followed by a 'T' or ' ' and a time.
  static std::optional<isoDate> parse(std::string_view dateStr);
  // Absent unless the parts name a real calendar day in year 1 or later.
  static std::optional<isoDate> fromParts(int year, int month, int day);
  static isoDate fromLocalTm(const std::tm& local);
  // Proleptic Gregorian day count with 1970-01-01 as day 0.
  static isoDate fromDayNumber(long dayNumber);

  [[nodiscard]] long dayNumber() const;
  // 0 = Monday ... 6 = Sunday.

  [[nodiscard]] std::string toString() const;

  [[nodiscard]] int year() const { return year_; }
  [[nodiscard]] int month() const { return month_; }
  [[nodiscard]] int day() const { return day_; }

  bool operator==(const isoDate& other) const {
    return year_ == other.year_ && month_ == other.month_ && day_ == other.day_;
  }
  bool operator!=(const isoDate& other) const { return !(*this == other); }
  bool operator<(const isoDate& other) const {
    if (year_ != other.year_)
      return year_ < other.year_;
    if (month_ != other.month_)
      return month_ < other.month_;
    return day_ < other.day_;
  }
  bool operator<=(const isoDate& other) const { return !(other < *this); }
  bool operator>(const isoDate& other) const { return other < *this; }
  bool operator>=(const isoDate& other) const { return !(*this < other); }

private:
  isoDate() = default;

  int year_ = 0;
  int month_ = 0;
  int day_ = 0;
};

}  // namespace catalist
#endif  // CATALIST_CORE_DATE_HPP_
