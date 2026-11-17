#include <doctest/doctest.h>

#include "web/scores.hpp"

using catalist::provider;
using catalist::web::outOfTen;
using catalist::web::stars;

namespace {

// Nerd Font stars: full, half, empty.
const std::string F = "";
const std::string H = "";
const std::string E = "";

}  // namespace

TEST_CASE("a score is five stars, a half star a point") {
	CHECK(stars(10) == F + F + F + F + F);
	CHECK(stars(7) == F + F + F + H + E);
	CHECK(stars(1) == H + E + E + E + E);
	CHECK(stars(8.5) == F + F + F + F + H);
	CHECK(stars(8.4) == F + F + F + F + E);
	CHECK(stars(0) == "-");
}

TEST_CASE("every site's score is out of 10 for the stars") {
	// AniList keeps every format out of 100: a three-point face of 85 too.
	CHECK(outOfTen(provider::kAniList, 85) == doctest::Approx(8.5));
	// Kitsu's ratingTwenty, whatever the rating system shows.
	CHECK(outOfTen(provider::kKitsu, 17) == doctest::Approx(8.5));
	CHECK(outOfTen(provider::kMyAnimeList, 7) == doctest::Approx(7));
}
