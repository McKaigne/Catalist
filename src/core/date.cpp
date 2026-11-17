#include "core/date.hpp"

#include <charconv>
#include <format>
#include <iomanip>
#include <sstream>

namespace catalist {

namespace {

std::optional<int> ParseDigits(std::string_view digits) {
	int number = 0;
	auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), number);
	if (error != std::errc() or end != digits.data() + digits.size() or digits.front() == '-' or
	    digits.front() == '+')
		return std::nullopt;
	return number;
}

bool IsLeapYear(int year) {
	return (year % 4 == 0 and year % 100 != 0) or year % 400 == 0;
}

int DaysInMonth(int year, int month) {
	static const int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	return month == 2 and IsLeapYear(year) ? 29 : kDays[month - 1];
}

bool HasDateShape(std::string_view text) {
	if (text.size() < 10 or text[4] != '-' or text[7] != '-')
		return false;
	return text.size() == 10 or text[10] == 'T' or text[10] == ' ';
}

}  // namespace

std::optional<IsoDate> IsoDate::Parse(std::string_view date_str) {
	if (not HasDateShape(date_str))
		return std::nullopt;
	auto year = ParseDigits(date_str.substr(0, 4));
	auto month = ParseDigits(date_str.substr(5, 2));
	auto day = ParseDigits(date_str.substr(8, 2));
	if (not year or not month or not day)
		return std::nullopt;
	return FromParts(*year, *month, *day);
}

std::optional<IsoDate> IsoDate::FromParts(int year, int month, int day) {
	if (year <= 0 or month < 1 or month > 12 or day < 1 or day > DaysInMonth(year, month))
		return std::nullopt;
	IsoDate date;
	date.year_ = year;
	date.month_ = month;
	date.day_ = day;
	return date;
}

IsoDate IsoDate::FromLocalTm(const std::tm& local) {
	IsoDate date;
	date.year_ = local.tm_year + 1900;
	date.month_ = local.tm_mon + 1;
	date.day_ = local.tm_mday;
	return date;
}

std::string IsoDate::ToString() const {
	std::ostringstream ss;
	ss << std::setfill(char(48)) << std::setw(4) << year_ << "-" << std::setw(2) << month_ << "-"
	   << std::setw(2) << day_;
	return ss.str();
}

std::string StampOf(const std::tm& local) {
	return std::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}", local.tm_year + 1900,
	                   local.tm_mon + 1, local.tm_mday, local.tm_hour, local.tm_min, local.tm_sec);
}

}  // namespace catalist
