#include <doctest/doctest.h>

#include "support/testSupport.hpp"
#include "web/kitsu.hpp"

namespace kitsu = catalist::kitsu;
using catalist::mediaType;

namespace {

// Kitsu's JSON:API replies, shortened: the title, and its mappings to the
// other sites in `included`.
const char* kFrieren = R"({"data":{"id":"46474","type":"anime","attributes":{
  "canonicalTitle":"Sousou no Frieren","episodeCount":28,"startDate":"2023-09-29",
  "averageRating":"89.6","status":"finished"},
  "relationships":{"mappings":{"data":[{"type":"mappings","id":"7"},{"type":"mappings","id":"9"}]}}},
  "included":[
  {"id":"7","type":"mappings","attributes":{"externalSite":"myanimelist/anime","externalId":"52991"}},
  {"id":"9","type":"mappings","attributes":{"externalSite":"anilist/anime","externalId":"154587"}},
  {"id":"8","type":"mappings","attributes":{"externalSite":"anidb","externalId":"17617"}}]})";

const char* kOnePiece = R"({"data":{"id":"12","type":"anime","attributes":{
  "canonicalTitle":"One Piece","episodeCount":null,"status":"current"},
  "relationships":{"mappings":{"data":[]}}},"included":[]})";

}  // namespace

TEST_CASE("kitsu reads a title, its totals and the other sites' ids") {
	auto found = kitsu::parseTitle(kFrieren, mediaType::kAnime);
	REQUIRE(found.ok());
	CHECK(found.value().name == "Sousou no Frieren");
	CHECK(found.value().facts.kitsuId == 46474);
	CHECK(found.value().facts.malId == 52991);
	CHECK(found.value().facts.anilistId == 154587);
	CHECK(found.value().facts.totalUnits == 28);
	CHECK(found.value().facts.released == 28);
	CHECK_FALSE(found.value().ongoing);
}

TEST_CASE("kitsu does not know how much of a running show is out") {
	auto found = kitsu::parseTitle(kOnePiece, mediaType::kAnime);
	REQUIRE(found.ok());
	CHECK(found.value().ongoing);
	CHECK(found.value().facts.totalUnits == 0);
	CHECK(found.value().facts.released == 0);
	CHECK_FALSE(kitsu::parseTitle(R"({"data":null})", mediaType::kAnime).ok());
	CHECK_FALSE(kitsu::parseTitle("<html>", mediaType::kAnime).ok());
}

TEST_CASE("kitsu searches by text, with the mappings of each match") {
	CHECK(kitsu::searchUrl(mediaType::kManga, "one piece") ==
	      "https://kitsu.app/api/edge/manga?filter[text]=one%20piece&page[limit]=10"
	      "&include=mappings");
	std::string reply = R"({"data":[{"id":"46474","type":"anime","attributes":{
	  "canonicalTitle":"Sousou no Frieren","episodeCount":28,"startDate":"2023-09-29",
	  "averageRating":"89.6"},"relationships":{"mappings":{"data":[{"type":"mappings","id":"7"}]}}},
	  {"id":"0","type":"anime","attributes":{}}],
	  "included":[{"id":"7","type":"mappings",
	  "attributes":{"externalSite":"myanimelist/anime","externalId":"52991"}}]})";
	auto found = kitsu::parseSearch(reply, mediaType::kAnime);
	REQUIRE(found.ok());
	REQUIRE(found.value().size() == 1);
	CHECK(found.value()[0].kitsuId == 46474);
	CHECK(found.value()[0].malId == 52991);
	CHECK(found.value()[0].anilistId == 0);
	CHECK(found.value()[0].year == "2023");
	CHECK(found.value()[0].score == doctest::Approx(8.96));
	CHECK_FALSE(kitsu::parseSearch("{}", mediaType::kAnime).ok());
}

TEST_CASE("kitsu finds a title by its MyAnimeList id through a mapping") {
	catalist::test::fakeWeb web;
	web.respond(
	    "GET https://kitsu.app/api/edge/mappings?filter[externalSite]=myanimelist/anime"
	    "&filter[externalId]=52991",
	    R"({"data":[{"id":"7","relationships":{"item":{"data":{"type":"anime","id":"46474"}}}}]})");
	web.respond("GET https://kitsu.app/api/edge/anime/46474?include=mappings", kFrieren);
	kitsu::tracker site(web.link());
	catalist::entry known;
	known.malId = 52991;
	auto found = site.lookUp(known);
	REQUIRE(found.ok());
	CHECK(found.value().facts.kitsuId == 46474);
	CHECK(web.sent.size() == 2);

	// A title MyAnimeList has and Kitsu does not.
	known.malId = 1;
	web.respond("filter[externalId]=1&", R"({"data":[]})");
	CHECK(site.lookUp(known).errorMessage() == "Kitsu does not know the title");
	CHECK(site.lookUp(catalist::entry{}).errorMessage() == "the title has no id to look up");
	// Offline.
	known.malId = 2;
	CHECK(site.lookUp(known).errorMessage() == "could not reach Kitsu");
}

TEST_CASE("kitsu looks titles up one at a time, and stops once it is not there") {
	catalist::test::fakeWeb web;
	web.respond("GET https://kitsu.app/api/edge/anime/46474?include=mappings", kFrieren);
	int pauses = 0;
	auto net = web.link();
	net.pause = [&pauses] { ++pauses; };
	kitsu::tracker site(net);
	catalist::entry frieren = catalist::test::makeEntry().kitsuId(46474);
	catalist::entry gone = catalist::test::makeEntry().kitsuId(1);
	auto found = site.lookUpMany({frieren, gone, frieren});
	REQUIRE(found.size() == 3);
	CHECK(found[0].ok());
	CHECK(found[1].errorMessage() == "could not reach Kitsu");
	CHECK(found[2].errorMessage() == "could not reach Kitsu");
	CHECK(web.sent.size() == 2);
	CHECK(pauses == 1);
}
