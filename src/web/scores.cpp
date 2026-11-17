#include "web/scores.hpp"

#include <algorithm>
#include <cmath>

namespace catalist::web {

namespace {

// Nerd Font's Font Awesome stars: nf-fa-star, nf-fa-star_half_o and
// nf-fa-star_o.
const char* const kFullStar = "";
const char* const kHalfStar = "";
const char* const kEmptyStar = "";

}  // namespace

std::string stars(double outOfTen) {
	int halves = std::clamp(static_cast<int>(std::lround(outOfTen)), 0, 10);
	if (halves == 0)
		return "-";
	std::string row;
	for (int star = 0; star < 5; ++star) {
		int left = halves - star * 2;
		row += left >= 2 ? kFullStar : left == 1 ? kHalfStar : kEmptyStar;
	}
	return row;
}

double outOfTen(provider site, double raw) {
	switch (site) {
		case provider::kAniList:
			return raw / 10.0;
		case provider::kKitsu:
			return raw / 2.0;
		case provider::kMyAnimeList:
			break;
	}
	return raw;
}

}  // namespace catalist::web
