#include "web/scores.hpp"

#include <algorithm>
#include <cmath>
#include <format>

namespace catalist::web {

namespace {

// Nerd Font's Font Awesome stars: nf-fa-star, nf-fa-star_half_o and
// nf-fa-star_o.
const char* const kFullStar = "";
const char* const kHalfStar = "";
const char* const kEmptyStar = "";

// `halves` half stars out of `count` stars, as a row of them.
std::string stars(int halves, int count) {
	halves = std::clamp(halves, 0, count * 2);
	std::string row;
	for (int star = 0; star < count; ++star) {
		int left = halves - star * 2;
		row += left >= 2 ? kFullStar : left == 1 ? kHalfStar : kEmptyStar;
	}
	return row;
}

// "8" or "8.5": one decimal, only when there is one.
std::string halfPoints(double value) {
	return std::trunc(value) == value ? std::format("{}", static_cast<int>(value))
	                                  : std::format("{:.1f}", value);
}

// AniList keeps every score out of 100, whatever the format.
std::string onAniList(const std::string& format, double raw) {
	if (format == "POINT_100")
		return std::format("{}", static_cast<int>(raw));
	if (format == "POINT_10_DECIMAL")
		return std::format("{:.1f}", raw / 10.0);
	if (format == "POINT_5")
		return stars(static_cast<int>((raw + 10.0) / 20.0) * 2, 5);
	// Three faces on AniList: one to three stars.
	if (format == "POINT_3")
		return stars((raw <= 35.0 ? 1 : raw <= 60.0 ? 2 : 3) * 2, 3);
	return std::format("{}", static_cast<int>(raw) / 10);
}

// Kitsu keeps ratingTwenty, 2 to 20, and rounds down to the format's steps.
std::string onKitsu(const std::string& format, double raw) {
	int twenty = static_cast<int>(raw);
	// Four faces on Kitsu: one to four stars.
	if (format == "simple")
		return stars((std::min(3, (twenty - 2) / 6) + 1) * 2, 4);
	// Half a star to five, in halves.
	if (format == "regular")
		return stars(twenty / 2, 5);
	return halfPoints(twenty / 2.0);
}

}  // namespace

std::string displayScore(provider site, const std::string& format, double raw) {
	if (raw <= 0.0)
		return "-";
	switch (site) {
		case provider::kAniList:
			return onAniList(format, raw);
		case provider::kKitsu:
			return onKitsu(format, raw);
		case provider::kMyAnimeList:
			break;
	}
	return std::format("{}", static_cast<int>(raw));
}

}  // namespace catalist::web
