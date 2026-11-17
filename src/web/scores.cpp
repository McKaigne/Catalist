#include "web/scores.hpp"

#include <algorithm>
#include <cmath>
#include <format>

namespace catalist::web {

namespace {

// "8" or "8.5": one decimal, only when there is one.
std::string halves(double value) {
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
		return std::format("{}/5", static_cast<int>((raw + 10.0) / 20.0));
	// AniList draws three faces: as stars, one to three.
	if (format == "POINT_3")
		return raw <= 35.0 ? "1/3" : raw <= 60.0 ? "2/3" : "3/3";
	return std::format("{}", static_cast<int>(raw) / 10);
}

// Kitsu keeps ratingTwenty, 2 to 20, and rounds down to the format's steps.
std::string onKitsu(const std::string& format, double raw) {
	int twenty = static_cast<int>(raw);
	// Kitsu draws four faces: as stars, one to four.
	if (format == "simple")
		return std::format("{}/4", std::min(3, (twenty - 2) / 6) + 1);
	if (format == "regular")
		return std::format("{}/5", halves((twenty / 2) / 2.0));
	return halves(twenty / 2.0);
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
