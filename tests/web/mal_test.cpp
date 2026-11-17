#include "web/mal.hpp"

#include <doctest/doctest.h>

#include "support/test_support.hpp"

namespace mal = catalist::mal;
using catalist::MediaType;

TEST_CASE("mal reads a title and its totals") {
	auto found = mal::ParseTitle(R"({"id":2,"title":"Berserk","num_chapters":0,"num_volumes":0,
	  "start_date":"1989-08-25","mean":9.47,"status":"currently_publishing"})",
	                             MediaType::kManga);
	REQUIRE(found.has_value());
	CHECK(found.value().name == "Berserk");
	CHECK(found.value().facts.mal_id == 2);
	CHECK(found.value().facts.anilist_id == 0);
	CHECK(found.value().facts.total_units == 0);
	CHECK(found.value().ongoing);

	auto frieren = mal::ParseTitle(R"({"id":52991,"title":"Sousou no Frieren","num_episodes":28,
	  "status":"finished_airing"})",
	                               MediaType::kAnime);
	REQUIRE(frieren.has_value());
	CHECK(frieren.value().facts.released == 28);
	CHECK_FALSE(mal::ParseTitle(R"({"error":"not_found"})", MediaType::kAnime).has_value());
}

TEST_CASE("mal searches with the fields of the list asked") {
	CHECK(mal::SearchUrl(MediaType::kAnime, "frieren & co") ==
	      "https://api.myanimelist.net/v2/anime?q=frieren%20%26%20co&limit=10"
	      "&fields=num_episodes,start_date,mean,status");
	auto found = mal::ParseSearch(R"({"data":[{"node":{"id":52991,"title":"Sousou no Frieren",
	  "num_episodes":28,"start_date":"2023-09-29","mean":9.3}},{"node":{"id":0}}],"paging":{}})",
	                              MediaType::kAnime);
	REQUIRE(found.has_value());
	REQUIRE(found.value().size() == 1);
	CHECK(found.value()[0].mal_id == 52991);
	CHECK(found.value()[0].total_units == 28);
	CHECK(found.value()[0].year == "2023");
	CHECK(found.value()[0].score == doctest::Approx(9.3));
}

TEST_CASE("mal sends the client id, and asks nothing without one") {
	catalist::test::FakeWeb web;
	auto net = web.Link();
	mal::Tracker without(net);
	auto refused = without.Search(MediaType::kAnime, "frieren");
	CHECK(refused.error().message.find("CATALIST_MAL_CLIENT_ID") != std::string::npos);
	CHECK(web.sent.empty());

	net.ids.mal = "abc";
	std::vector<std::string> headers;
	net.fetch = [&headers](const catalist::web::Request& sent) {
		headers = sent.headers;
		return std::optional<catalist::web::Reply>({200, R"({"data":[]})"});
	};
	mal::Tracker with(net);
	CHECK(with.Search(MediaType::kAnime, "frieren").has_value());
	CHECK(headers == std::vector<std::string>{"X-MAL-CLIENT-ID: abc"});
}

TEST_CASE("mal tells a title it does not know from one it cannot reach") {
	catalist::test::FakeWeb web;
	auto net = web.Link();
	net.ids.mal = "abc";
	web.Respond("anime/1?", R"({"error":"not_found","message":""})", 404);
	web.Respond("anime/52991?", R"({"id":52991,"title":"Sousou no Frieren","num_episodes":28,
	  "status":"finished_airing"})");
	mal::Tracker site(net);
	std::vector<catalist::Entry> known(2);
	known[0].mal_id = 1;
	known[1].mal_id = 52991;
	// A 404 is about that title only: the next one is still asked.
	auto found = site.LookUpMany(known);
	REQUIRE(found.size() == 2);
	CHECK(found[0].error().why == catalist::web::Failure::Kind::kRefused);
	CHECK(found[0].error().message == "MyAnimeList does not know the title");
	REQUIRE(found[1].has_value());
	CHECK(found[1].value().facts.total_units == 28);
	// A site that is down is asked no more.
	web.Respond("anime/1?", "", 503);
	found = site.LookUpMany(known);
	CHECK(found[0].error().why == catalist::web::Failure::Kind::kUnreachable);
	CHECK(found[1].error().why == catalist::web::Failure::Kind::kUnreachable);
}
