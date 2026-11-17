#include "web/kitsu.hpp"

#include <doctest/doctest.h>

#include "support/test_support.hpp"

namespace kitsu = catalist::kitsu;
using catalist::MediaType;

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
	auto found = kitsu::ParseTitle(kFrieren, MediaType::kAnime);
	REQUIRE(found.has_value());
	CHECK(found.value().name == "Sousou no Frieren");
	CHECK(found.value().facts.kitsu_id == 46474);
	CHECK(found.value().facts.mal_id == 52991);
	CHECK(found.value().facts.anilist_id == 154587);
	CHECK(found.value().facts.total_units == 28);
	CHECK(found.value().facts.released == 28);
	CHECK_FALSE(found.value().ongoing);
}

TEST_CASE("kitsu does not know how much of a running show is out") {
	auto found = kitsu::ParseTitle(kOnePiece, MediaType::kAnime);
	REQUIRE(found.has_value());
	CHECK(found.value().ongoing);
	CHECK(found.value().facts.total_units == 0);
	CHECK(found.value().facts.released == 0);
	CHECK_FALSE(kitsu::ParseTitle(R"({"data":null})", MediaType::kAnime).has_value());
	CHECK_FALSE(kitsu::ParseTitle("<html>", MediaType::kAnime).has_value());
}

TEST_CASE("kitsu searches by text, with the mappings of each match") {
	CHECK(kitsu::SearchUrl(MediaType::kManga, "one piece") ==
	      "https://kitsu.app/api/edge/manga?filter[text]=one%20piece&page[limit]=10"
	      "&include=mappings");
	std::string reply = R"({"data":[{"id":"46474","type":"anime","attributes":{
	  "canonicalTitle":"Sousou no Frieren","episodeCount":28,"startDate":"2023-09-29",
	  "averageRating":"89.6"},"relationships":{"mappings":{"data":[{"type":"mappings","id":"7"}]}}},
	  {"id":"0","type":"anime","attributes":{}}],
	  "included":[{"id":"7","type":"mappings",
	  "attributes":{"externalSite":"myanimelist/anime","externalId":"52991"}}]})";
	auto found = kitsu::ParseSearch(reply, MediaType::kAnime);
	REQUIRE(found.has_value());
	REQUIRE(found.value().size() == 1);
	CHECK(found.value()[0].kitsu_id == 46474);
	CHECK(found.value()[0].mal_id == 52991);
	CHECK(found.value()[0].anilist_id == 0);
	CHECK(found.value()[0].year == "2023");
	CHECK(found.value()[0].score == doctest::Approx(8.96));
	CHECK_FALSE(kitsu::ParseSearch("{}", MediaType::kAnime).has_value());
}

TEST_CASE("kitsu finds a title by its MyAnimeList id through a mapping") {
	catalist::test::FakeWeb web;
	web.Respond(
	    "GET https://kitsu.app/api/edge/mappings?filter[externalSite]=myanimelist/anime"
	    "&filter[externalId]=52991",
	    R"({"data":[{"id":"7","relationships":{"item":{"data":{"type":"anime","id":"46474"}}}}]})");
	web.Respond("GET https://kitsu.app/api/edge/anime/46474?include=mappings", kFrieren);
	kitsu::Tracker site(web.Link());
	catalist::Entry known;
	known.mal_id = 52991;
	auto found = site.LookUp(known);
	REQUIRE(found.has_value());
	CHECK(found.value().facts.kitsu_id == 46474);
	CHECK(web.sent.size() == 2);

	// A title MyAnimeList has and Kitsu does not.
	known.mal_id = 1;
	web.Respond("filter[externalId]=1&", R"({"data":[]})");
	CHECK(site.LookUp(known).error().message == "Kitsu does not know the title");
	CHECK(site.LookUp(known).error().why == catalist::web::Failure::Kind::kRefused);
	CHECK(site.LookUp(catalist::Entry{}).error().message == "the title has no id to look up");
	// Offline.
	known.mal_id = 2;
	CHECK(site.LookUp(known).error().message == "could not reach Kitsu");
	CHECK(site.LookUp(known).error().why == catalist::web::Failure::Kind::kUnreachable);
}

TEST_CASE("kitsu looks titles up one at a time, and stops once it is not there") {
	catalist::test::FakeWeb web;
	web.Respond("GET https://kitsu.app/api/edge/anime/46474?include=mappings", kFrieren);
	int pauses = 0;
	auto net = web.Link();
	net.pause = [&pauses] { ++pauses; };
	kitsu::Tracker site(net);
	catalist::Entry frieren = catalist::test::MakeEntry().KitsuId(46474);
	catalist::Entry gone = catalist::test::MakeEntry().KitsuId(1);
	auto found = site.LookUpMany({frieren, gone, frieren});
	REQUIRE(found.size() == 3);
	CHECK(found[0].has_value());
	CHECK(found[1].error().message == "could not reach Kitsu");
	CHECK(found[2].error().message == "could not reach Kitsu");
	CHECK(web.sent.size() == 2);
	CHECK(pauses == 1);
}
