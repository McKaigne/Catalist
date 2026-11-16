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

	[[nodiscard]] std::string toString() const;

	[[nodiscard]] int year() const { return year_; }
	[[nodiscard]] int month() const { return month_; }
	[[nodiscard]] int day() const { return day_; }

	bool operator==(const isoDate& other) const {
		return year_ == other.year_ and month_ == other.month_ and day_ == other.day_;
	}
	bool operator!=(const isoDate& other) const { return not(*this == other); }
	bool operator<(const isoDate& other) const {
		if (year_ != other.year_)
			return year_ < other.year_;
		if (month_ != other.month_)
			return month_ < other.month_;
		return day_ < other.day_;
	}
	bool operator<=(const isoDate& other) const { return not(other < *this); }
	bool operator>(const isoDate& other) const { return other < *this; }
	bool operator>=(const isoDate& other) const { return not(*this < other); }

private:
	isoDate() = default;

	int year_ = 0;
	int month_ = 0;
	int day_ = 0;
};

// "2026-10-20 21:30:00": a local date and time, as a step of the undo
// history is stamped.
[[nodiscard]] std::string stampOf(const std::tm& local);

}  // namespace catalist

#endif  // CATALIST_CORE_DATE_HPP_
