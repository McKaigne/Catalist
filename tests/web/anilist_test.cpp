#include "web/anilist.hpp"

#include <doctest/doctest.h>

#include "web/json.hpp"

namespace anilist = catalist::anilist;
using catalist::MediaType;

TEST_CASE("anilist escapes the search") {
	auto search = anilist::SearchRequest(MediaType::kAnime, "say \"hi\"\\\n");
	CHECK(search.find(R"("search":"say \"hi\"\\\u000a")") != std::string::npos);
	CHECK(catalist::json::Parse(search).has_value());
}

TEST_CASE("anilist search keeps titles MyAnimeList does not list") {
	auto found = anilist::ParseSearch(
	    R"({"data":{"Page":{"media":[
	      {"id":5114,"idMal":5114,"title":{"romaji":"Fullmetal Alchemist: Brotherhood"},
	       "episodes":64,"volumes":27,"averageScore":90,"startDate":{"year":2009}},
	      {"id":999001,"idMal":null,"title":{"romaji":"AniList only"},"episodes":3},
	      {"id":null,"title":{"romaji":"No id at all"}}]}}})",
	    MediaType::kAnime);
	REQUIRE(found.has_value());
	REQUIRE(found.value().size() == 2);
	CHECK(found.value()[0].anilist_id == 5114);
	CHECK(found.value()[0].mal_id == 5114);
	CHECK(found.value()[0].total_units == 64);
	CHECK(found.value()[0].total_volumes == 27);
	CHECK(found.value()[0].year == "2009");
	CHECK(found.value()[0].score == doctest::Approx(9.0));
	CHECK(found.value()[1].anilist_id == 999001);
	CHECK(found.value()[1].mal_id == 0);
	CHECK(found.value()[1].year.empty());
	CHECK(found.value()[1].score == 0.0);
}
