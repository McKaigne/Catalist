#include <doctest/doctest.h>

#include "support/testSupport.hpp"
#include "web/mal.hpp"

namespace mal = catalist::mal;
using catalist::mediaType;

TEST_CASE("mal reads a title and its totals") {
	auto found = mal::parseTitle(R"({"id":2,"title":"Berserk","num_chapters":0,"num_volumes":0,
	  "start_date":"1989-08-25","mean":9.47,"status":"currently_publishing"})",
	                             mediaType::kManga);
	REQUIRE(found.has_value());
	CHECK(found.value().name == "Berserk");
	CHECK(found.value().facts.malId == 2);
	CHECK(found.value().facts.anilistId == 0);
	CHECK(found.value().facts.totalUnits == 0);
	CHECK(found.value().ongoing);

	auto frieren = mal::parseTitle(R"({"id":52991,"title":"Sousou no Frieren","num_episodes":28,
	  "status":"finished_airing"})",
	                               mediaType::kAnime);
	REQUIRE(frieren.has_value());
	CHECK(frieren.value().facts.released == 28);
	CHECK_FALSE(mal::parseTitle(R"({"error":"not_found"})", mediaType::kAnime).has_value());
}

TEST_CASE("mal searches with the fields of the list asked") {
	CHECK(mal::searchUrl(mediaType::kAnime, "frieren & co") ==
	      "https://api.myanimelist.net/v2/anime?q=frieren%20%26%20co&limit=10"
	      "&fields=num_episodes,start_date,mean,status");
	auto found = mal::parseSearch(R"({"data":[{"node":{"id":52991,"title":"Sousou no Frieren",
	  "num_episodes":28,"start_date":"2023-09-29","mean":9.3}},{"node":{"id":0}}],"paging":{}})",
	                              mediaType::kAnime);
	REQUIRE(found.has_value());
	REQUIRE(found.value().size() == 1);
	CHECK(found.value()[0].malId == 52991);
	CHECK(found.value()[0].totalUnits == 28);
	CHECK(found.value()[0].year == "2023");
	CHECK(found.value()[0].score == doctest::Approx(9.3));
}

TEST_CASE("mal sends the client id, and asks nothing without one") {
	catalist::test::fakeWeb web;
	auto net = web.link();
	mal::tracker without(net);
	auto refused = without.search(mediaType::kAnime, "frieren");
	CHECK(refused.error().find("CATALIST_MAL_CLIENT_ID") != std::string::npos);
	CHECK(web.sent.empty());

	net.ids.mal = "abc";
	std::vector<std::string> headers;
	net.fetch = [&headers](const catalist::web::request& sent) {
		headers = sent.headers;
		return std::optional<std::string>(R"({"data":[]})");
	};
	mal::tracker with(net);
	CHECK(with.search(mediaType::kAnime, "frieren").has_value());
	CHECK(headers == std::vector<std::string>{"X-MAL-CLIENT-ID: abc"});
}
