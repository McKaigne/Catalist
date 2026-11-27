#include "menu/find.hpp"

#include <doctest/doctest.h>

#include "support/test_support.hpp"

using catalist::test::MakeEntry;

TEST_CASE("find takes any part of a title, words in any order") {
	catalist::Entry taxi = MakeEntry().Title("Odd Taxi");
	CHECK(catalist::TitleHolds(taxi, "taxi"));
	CHECK(catalist::TitleHolds(taxi, "TAXI odd"));
	CHECK(catalist::TitleHolds(taxi, "dd ax"));
	CHECK_FALSE(catalist::TitleHolds(taxi, "naruto"));
	CHECK_FALSE(catalist::TitleHolds(taxi, "taxi naruto"));
}

TEST_CASE("find matches the English name too, but each name on its own") {
	catalist::Entry shown =
	    MakeEntry().Title("Sousou no Frieren").EnglishTitle("Frieren: Beyond Journey's End");
	CHECK(catalist::TitleHolds(shown, "sousou"));
	CHECK(catalist::TitleHolds(shown, "frieren beyond journeys end"));
	CHECK(catalist::TitleHolds(shown, "END, journey's"));
	CHECK_FALSE(catalist::TitleHolds(shown, "sousou beyond"));
}

TEST_CASE("find offers the closest title to a typo") {
	std::vector<catalist::Entry> titles = {MakeEntry().Title("Sousou no Frieren"),
	                                       MakeEntry().Title("Odd Taxi"),
	                                       MakeEntry().Title("Berserk")};
	CHECK(catalist::DisplayName(*catalist::ClosestTitle(titles, "bersrek")) == "Berserk");
	CHECK(catalist::DisplayName(*catalist::ClosestTitle(titles, "freiren")) == "Sousou no Frieren");
	CHECK_FALSE(catalist::ClosestTitle(titles, "naruto").has_value());
	CHECK_FALSE(catalist::ClosestTitle(titles, "").has_value());
}
