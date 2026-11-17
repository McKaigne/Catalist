#include <doctest/doctest.h>

#include "web/scores.hpp"

using catalist::provider;
using catalist::web::displayScore;

TEST_CASE("scores show in the person's format on AniList, out of 100 underneath") {
	CHECK(displayScore(provider::kAniList, "POINT_100", 85) == "85");
	CHECK(displayScore(provider::kAniList, "POINT_10_DECIMAL", 85) == "8.5");
	CHECK(displayScore(provider::kAniList, "POINT_10", 85) == "8");
	CHECK(displayScore(provider::kAniList, "POINT_5", 70) == "4/5");
	CHECK(displayScore(provider::kAniList, "POINT_3", 35) == ":(");
	CHECK(displayScore(provider::kAniList, "POINT_3", 60) == ":|");
	CHECK(displayScore(provider::kAniList, "POINT_3", 61) == ":)");
	// A format not known is read as out of 10.
	CHECK(displayScore(provider::kAniList, "", 90) == "9");
	CHECK(displayScore(provider::kAniList, "POINT_100", 0) == "-");
}

TEST_CASE("scores show in the person's rating system on Kitsu, 2 to 20 underneath") {
	CHECK(displayScore(provider::kKitsu, "advanced", 17) == "8.5");
	CHECK(displayScore(provider::kKitsu, "advanced", 16) == "8");
	CHECK(displayScore(provider::kKitsu, "regular", 15) == "3.5/5");
	CHECK(displayScore(provider::kKitsu, "regular", 20) == "5/5");
	CHECK(displayScore(provider::kKitsu, "simple", 7) == "awful");
	CHECK(displayScore(provider::kKitsu, "simple", 8) == "meh");
	CHECK(displayScore(provider::kKitsu, "simple", 20) == "great");
	CHECK(displayScore(provider::kKitsu, "simple", 0) == "-");
}

TEST_CASE("MyAnimeList scores are out of 10") {
	CHECK(displayScore(provider::kMyAnimeList, "", 7) == "7");
	CHECK(displayScore(provider::kMyAnimeList, "", 0) == "-");
}
