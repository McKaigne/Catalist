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

std::string isoDate::toString() const {
  std::ostringstream ss;
  ss << std::setfill(char(48)) << std::setw(4) << year_ << "-" << std::setw(2)
     << month_ << "-" << std::setw(2) << day_;
  return ss.str();
}

}  // namespace catalist
