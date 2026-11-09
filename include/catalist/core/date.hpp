#ifndef CATALIST_CORE_DATE_HPP_
#define CATALIST_CORE_DATE_HPP_

#include <optional>
#include <string>
#include <string_view>

namespace catalist {

class isoDate {
public:
  static std::optional<isoDate> parse(std::string_view dateStr);
  static isoDate today();

  static std::optional<isoDate> Parse(std::string_view dateStr) {
    return parse(dateStr);
  }
  static isoDate Today() { return today(); }

  [[nodiscard]] std::string toString() const;
  [[nodiscard]] std::string ToString() const { return toString(); }

  [[nodiscard]] int year() const { return year_; }
  [[nodiscard]] int month() const { return month_; }
  [[nodiscard]] int day() const { return day_; }
  [[nodiscard]] bool isEmpty() const { return year_ == 0; }

  [[nodiscard]] int Year() const { return year(); }
  [[nodiscard]] int Month() const { return month(); }
  [[nodiscard]] int Day() const { return day(); }
  [[nodiscard]] bool IsEmpty() const { return isEmpty(); }

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
  int year_ = 0;
  int month_ = 0;
  int day_ = 0;
};

}  // namespace catalist
#endif  // CATALIST_CORE_DATE_HPP_
