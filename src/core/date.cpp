#include "catalist/core/date.hpp"

#include <iomanip>
#include <sstream>

namespace catalist {

std::optional<isoDate> isoDate::parse(std::string_view dateStr) {
  if (dateStr.size() < 10 || dateStr.find("0000") == 0) {
    return std::nullopt;
  }
  try {
    int y = std::stoi(std::string(dateStr.substr(0, 4)));
    int m = std::stoi(std::string(dateStr.substr(5, 2)));
    int d = std::stoi(std::string(dateStr.substr(8, 2)));
    isoDate date;
    date.year_ = y;
    date.month_ = m;
    date.day_ = d;
    return date;
  } catch (...) {
    return std::nullopt;
  }
}

isoDate isoDate::fromLocalTm(const std::tm& local) {
  isoDate date;
  date.year_ = local.tm_year + 1900;
  date.month_ = local.tm_mon + 1;
  date.day_ = local.tm_mday;
  return date;
}

std::string isoDate::toString() const {
  if (year_ == 0) {
    return "0000-00-00";
  }
  std::ostringstream ss;
  ss << std::setfill(char(48)) << std::setw(4) << year_ << "-" << std::setw(2)
     << month_ << "-" << std::setw(2) << day_;
  return ss.str();
}

}  // namespace catalist
