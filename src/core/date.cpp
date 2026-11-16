#include "core/date.hpp"

#include <charconv>
#include <format>
#include <iomanip>
#include <sstream>

namespace catalist {

namespace {

std::optional<int> parseDigits(std::string_view digits) {
	int number = 0;
	auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), number);
	if (error != std::errc() or end != digits.data() + digits.size() or digits.front() == '-' or
	    digits.front() == '+')
		return std::nullopt;
	return number;
}

bool isLeapYear(int year) {
	return (year % 4 == 0 and year % 100 != 0) or year % 400 == 0;
}

int daysInMonth(int year, int month) {
	static const int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	return month == 2 and isLeapYear(year) ? 29 : kDays[month - 1];
}

bool hasDateShape(std::string_view text) {
	if (text.size() < 10 or text[4] != '-' or text[7] != '-')
		return false;
	return text.size() == 10 or text[10] == 'T' or text[10] == ' ';
}

}  // namespace

std::optional<isoDate> isoDate::parse(std::string_view dateStr) {
	if (not hasDateShape(dateStr))
		return std::nullopt;
	auto year = parseDigits(dateStr.substr(0, 4));
	auto month = parseDigits(dateStr.substr(5, 2));
	auto day = parseDigits(dateStr.substr(8, 2));
	if (not year or not month or not day)
		return std::nullopt;
	return fromParts(*year, *month, *day);
}

std::optional<isoDate> isoDate::fromParts(int year, int month, int day) {
	if (year <= 0 or month < 1 or month > 12 or day < 1 or day > daysInMonth(year, month))
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

std::string isoDate::toString() const {
	std::ostringstream ss;
	ss << std::setfill(char(48)) << std::setw(4) << year_ << "-" << std::setw(2) << month_ << "-"
	   << std::setw(2) << day_;
	return ss.str();
}

std::string stampOf(const std::tm& local) {
	return std::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}", local.tm_year + 1900,
	                   local.tm_mon + 1, local.tm_mday, local.tm_hour, local.tm_min, local.tm_sec);
}

}  // namespace catalist
