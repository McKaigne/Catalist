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

	bool operator==(const IsoDate& other) const {
		return year_ == other.year_ and month_ == other.month_ and day_ == other.day_;
	}

private:
	IsoDate() = default;

	int year_ = 0;
	int month_ = 0;
	int day_ = 0;
};

[[nodiscard]] std::string StampOf(const std::tm& local);

}  // namespace catalist

#endif  // CATALIST_CORE_DATE_HPP_
