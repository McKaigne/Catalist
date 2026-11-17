#include <doctest/doctest.h>

#include "web/scores.hpp"

using catalist::provider;
using catalist::web::displayScore;

namespace {

// Nerd Font stars: full, half, empty.
const std::string F = "\uf005";
const std::string H = "\uf123";
const std::string E = "\uf006";

}  // namespace

TEST_CASE("scores show in the person's format on AniList, out of 100 underneath") {
	CHECK(displayScore(provider::kAniList, "POINT_100", 85) == "85");
	CHECK(displayScore(provider::kAniList, "POINT_10_DECIMAL", 85) == "8.5");
	CHECK(displayScore(provider::kAniList, "POINT_10", 85) == "8");
	CHECK(displayScore(provider::kAniList, "POINT_5", 70) == F + F + F + F + E);
	CHECK(displayScore(provider::kAniList, "POINT_3", 35) == F + E + E);
	CHECK(displayScore(provider::kAniList, "POINT_3", 60) == F + F + E);
	CHECK(displayScore(provider::kAniList, "POINT_3", 61) == F + F + F);
	// A format not known is read as out of 10.
	CHECK(displayScore(provider::kAniList, "", 90) == "9");
	CHECK(displayScore(provider::kAniList, "POINT_100", 0) == "-");
}

TEST_CASE("scores show in the person's rating system on Kitsu, 2 to 20 underneath") {
	CHECK(displayScore(provider::kKitsu, "advanced", 17) == "8.5");
	CHECK(displayScore(provider::kKitsu, "advanced", 16) == "8");
	CHECK(displayScore(provider::kKitsu, "regular", 15) == F + F + F + H + E);
	CHECK(displayScore(provider::kKitsu, "regular", 20) == F + F + F + F + F);
	CHECK(displayScore(provider::kKitsu, "simple", 7) == F + E + E + E);
	CHECK(displayScore(provider::kKitsu, "simple", 8) == F + F + E + E);
	CHECK(displayScore(provider::kKitsu, "simple", 20) == F + F + F + F);
	CHECK(displayScore(provider::kKitsu, "simple", 0) == "-");
}

TEST_CASE("MyAnimeList scores are out of 10") {
	CHECK(displayScore(provider::kMyAnimeList, "", 7) == "7");
	CHECK(displayScore(provider::kMyAnimeList, "", 0) == "-");
}
