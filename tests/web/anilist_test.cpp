#include "web/anilist.hpp"

#include <doctest/doctest.h>

#include "web/json.hpp"

namespace anilist = catalist::anilist;
using catalist::MediaType;

TEST_CASE("anilist escapes the search") {
	auto search = anilist::SearchRequest(std::nullopt, "say \"hi\"\\\n");
	CHECK(search.find(R"("search":"say \"hi\"\\\u000a")") != std::string::npos);
	CHECK(catalist::json::Parse(search).has_value());
}

TEST_CASE("anilist search keeps titles MyAnimeList does not list") {
	auto found = anilist::ParseSearch(
	    R"({"data":{"anime":{"media":[
	      {"id":5114,"idMal":5114,"title":{"romaji":"Fullmetal Alchemist: Brotherhood"},
	       "episodes":64,"volumes":27,"format":"TV","averageScore":90,"startDate":{"year":2009}},
	      {"id":999001,"idMal":null,"title":{"romaji":"AniList only"},"episodes":3,
	       "format":"TV_SHORT"},
	      {"id":null,"title":{"romaji":"No id at all"}}]}}})");
	REQUIRE(found.has_value());
	REQUIRE(found.value().size() == 2);
	CHECK(found.value()[0].anilist_id == 5114);
	CHECK(found.value()[0].mal_id == 5114);
	CHECK(found.value()[0].total_units == 64);
	CHECK(found.value()[0].total_volumes == 27);
	CHECK(found.value()[0].format == catalist::MediaFormat::kTv);
	CHECK(found.value()[0].year == "2009");
	CHECK(found.value()[0].score == doctest::Approx(9.0));
	CHECK(found.value()[1].anilist_id == 999001);
	CHECK(found.value()[1].mal_id == 0);
	CHECK(found.value()[1].format == catalist::MediaFormat::kTvShort);
	CHECK(found.value()[1].year.empty());
	CHECK(found.value()[1].score == 0.0);
}

TEST_CASE("anilist asks for five of each type, or ten of one, and no adult titles") {
	auto both = anilist::SearchRequest(std::nullopt, "x");
	CHECK(both.find("anime:Page(perPage:5)") != std::string::npos);
	CHECK(both.find("manga:Page(perPage:5)") != std::string::npos);
	CHECK(both.find("isAdult:false") != std::string::npos);
	CHECK(both.find("romaji english") != std::string::npos);
	CHECK(both.find(" format ") != std::string::npos);
	CHECK(catalist::json::Parse(both).has_value());
	auto manga = anilist::SearchRequest(MediaType::kManga, "x");
	CHECK(manga.find("manga:Page(perPage:10)") != std::string::npos);
	CHECK(manga.find("anime:Page") == std::string::npos);
}

TEST_CASE("anilist reads anime before manga, with their English names") {
	auto found = anilist::ParseSearch(R"({"data":{
	  "manga":{"media":[{"id":2,"title":{"romaji":"M"},"chapters":9,"format":"MANGA"}]},
	  "anime":{"media":[{"id":1,"title":{"romaji":"A","english":"Eh"},"episodes":3}]}}})");
	REQUIRE(found.has_value());
	REQUIRE(found.value().size() == 2);
	CHECK(found.value()[0].type == MediaType::kAnime);
	CHECK(found.value()[0].english == "Eh");
	CHECK(found.value()[0].total_units == 3);
	CHECK(found.value()[1].type == MediaType::kManga);
	CHECK(found.value()[1].total_units == 9);
	CHECK(found.value()[1].format == catalist::MediaFormat::kUnknown);
	CHECK_FALSE(anilist::ParseSearch(R"({"data":{}})").has_value());
	CHECK_FALSE(anilist::ParseSearch("not json").has_value());
}
