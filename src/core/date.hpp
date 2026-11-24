#ifndef CATALIST_CORE_DATE_HPP_
#define CATALIST_CORE_DATE_HPP_

#include <ctime>
#include <optional>
#include <string>
#include <string_view>

namespace catalist {

class IsoDate {
public:
	static std::optional<IsoDate> Parse(std::string_view date_str);
	static std::optional<IsoDate> FromParts(int year, int month, int day);
	static IsoDate FromLocalTm(const std::tm& local);

	[[nodiscard]] std::string ToString() const;

	[[nodiscard]] int Year() const { return year_; }
	[[nodiscard]] int Month() const { return month_; }
	[[nodiscard]] int Day() const { return day_; }

	bool operator==(const IsoDate& other) const {
		return year_ == other.year_ and month_ == other.month_ and day_ == other.day_;
	}
	bool operator!=(const IsoDate& other) const { return not(*this == other); }
	bool operator<(const IsoDate& other) const {
		if (year_ != other.year_)
			return year_ < other.year_;
		if (month_ != other.month_)
			return month_ < other.month_;
		return day_ < other.day_;
	}
	bool operator<=(const IsoDate& other) const { return not(other < *this); }
	bool operator>(const IsoDate& other) const { return other < *this; }
	bool operator>=(const IsoDate& other) const { return not(*this < other); }

private:
	IsoDate() = default;

	int year_ = 0;
	int month_ = 0;
	int day_ = 0;
};

[[nodiscard]] std::string StampOf(const std::tm& local);

}  // namespace catalist

#endif  // CATALIST_CORE_DATE_HPP_
