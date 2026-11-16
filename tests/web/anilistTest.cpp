#include <doctest/doctest.h>

#include "support/testSupport.hpp"
#include "web/anilist.hpp"
#include "web/json.hpp"

namespace anilist = catalist::anilist;
using catalist::mediaType;

namespace {

// Real replies, shortened: Frieren (finished), One Piece (airing) and
// Berserk (a manga still running).
const char* kFrieren =
    R"({"data":{"Media":{"title":{"romaji":"Sousou no Frieren"},"episodes":28,"chapters":null,"volumes":null,
  "status":"FINISHED","nextAiringEpisode":null}}})";
const char* kOnePiece = R"({"data":{"Media":{"episodes":null,"chapters":null,"volumes":null,
  "status":"RELEASING","nextAiringEpisode":{"episode":1181}}}})";
const char* kBerserk = R"({"data":{"Media":{"episodes":null,"chapters":null,"volumes":null,
  "status":"RELEASING"}}})";

}  // namespace

TEST_CASE("anilist reads a finished title and its totals") {
	auto found = anilist::parseLookup(kFrieren, mediaType::kAnime);
	REQUIRE(found.ok());
	CHECK(found.value().name == "Sousou no Frieren");
	CHECK(found.value().facts.totalUnits == 28);
	CHECK(found.value().facts.released == 28);
	CHECK_FALSE(found.value().ongoing);
}

TEST_CASE("anilist counts the aired episodes from the next one to air") {
	auto found = anilist::parseLookup(kOnePiece, mediaType::kAnime);
	REQUIRE(found.ok());
	CHECK(found.value().ongoing);
	CHECK(found.value().facts.totalUnits == 0);
	CHECK(found.value().facts.released == 1180);
}

TEST_CASE("anilist leaves a running manga's chapters unknown") {
	auto found = anilist::parseLookup(kBerserk, mediaType::kManga);
	REQUIRE(found.ok());
	CHECK(found.value().ongoing);
	CHECK(found.value().facts.released == 0);
	CHECK_FALSE(anilist::parseLookup(R"({"data":{"Media":null}})", mediaType::kManga).ok());
	CHECK_FALSE(anilist::parseLookup("<html>", mediaType::kManga).ok());
}

TEST_CASE("anilist asks by its own id when known, else by the MyAnimeList id") {
	auto byMal = anilist::lookupRequest({mediaType::kManga, 0, 2});
	CHECK(byMal.find("Media(idMal:$id") != std::string::npos);
	CHECK(byMal.find(R"("variables":{"id":2,"type":"MANGA"})") != std::string::npos);
	auto own = anilist::lookupRequest({mediaType::kAnime, 154587, 52991});
	CHECK(own.find("Media(id:$id") != std::string::npos);
	CHECK(own.find(R"("id":154587)") != std::string::npos);
	CHECK(catalist::json::parse(own).ok());

	catalist::test::fakeWeb web;
	// Without an id there is nothing to ask.
	anilist::tracker site(web.link());
	CHECK_FALSE(site.lookUp(catalist::entry{}).ok());
	CHECK(web.sent.empty());
}

TEST_CASE("anilist reads both ids from a lookup") {
	auto found = anilist::parseLookup(R"({"data":{"Media":{"id":154587,"idMal":52991}}})",
	                                  mediaType::kAnime);
	REQUIRE(found.ok());
	CHECK(found.value().facts.anilistId == 154587);
	CHECK(found.value().facts.malId == 52991);
}

TEST_CASE("anilist escapes the search") {
	auto search = anilist::searchRequest(mediaType::kAnime, "say \"hi\"\\\n");
	CHECK(search.find(R"("search":"say \"hi\"\\\u000a")") != std::string::npos);
	CHECK(catalist::json::parse(search).ok());
}

TEST_CASE("anilist search keeps titles MyAnimeList does not list") {
	auto found = anilist::parseSearch(
	    R"({"data":{"Page":{"media":[
	      {"id":5114,"idMal":5114,"title":{"romaji":"Fullmetal Alchemist: Brotherhood"},
	       "episodes":64,"averageScore":90,"startDate":{"year":2009}},
	      {"id":999001,"idMal":null,"title":{"romaji":"AniList only"},"episodes":3},
	      {"id":null,"title":{"romaji":"No id at all"}}]}}})",
	    mediaType::kAnime);
	REQUIRE(found.ok());
	REQUIRE(found.value().size() == 2);
	CHECK(found.value()[0].anilistId == 5114);
	CHECK(found.value()[0].malId == 5114);
	CHECK(found.value()[0].totalUnits == 64);
	CHECK(found.value()[0].year == "2009");
	CHECK(found.value()[0].score == doctest::Approx(9.0));
	CHECK(found.value()[1].anilistId == 999001);
	CHECK(found.value()[1].malId == 0);
	CHECK(found.value()[1].year.empty());
	CHECK(found.value()[1].score == 0.0);
}

TEST_CASE("anilist looks up many titles in one request, by either id") {
	auto request = anilist::batchRequest(mediaType::kAnime, {154587, 128547}, false);
	CHECK(request.find("media(id_in:$ids") != std::string::npos);
	CHECK(request.find(R"("ids":[154587,128547],"type":"ANIME")") != std::string::npos);
	CHECK(catalist::json::parse(request).ok());
	CHECK(anilist::batchRequest(mediaType::kManga, {2}, true).find("idMal_in:$ids") !=
	      std::string::npos);

	catalist::test::fakeWeb web;
	web.respond(R"("ids":[154587,128547],"type":"ANIME")",
	            R"({"data":{"Page":{"media":[
	            {"id":128547,"idMal":44265,"episodes":13,"status":"FINISHED"},
	            {"id":154587,"idMal":52991,"episodes":28,"status":"FINISHED"}]}}})");
	web.respond(R"("ids":[2],"type":"MANGA")",
	            R"({"data":{"Page":{"media":[{"id":30002,"idMal":2,"status":"RELEASING"}]}}})");
	int pauses = 0;
	auto net = web.link();
	net.pause = [&pauses] { ++pauses; };
	anilist::tracker site(net);
	std::vector<catalist::entry> titles = {
	    catalist::test::makeEntry().anilistId(154587),
	    catalist::test::makeEntry().type(mediaType::kManga).malId(2),
	    catalist::test::makeEntry().anilistId(128547),
	    catalist::test::makeEntry().title("Home Video"),
	};
	auto found = site.lookUpMany(titles);
	REQUIRE(found.size() == 4);
	CHECK(found[0].value().facts.totalUnits == 28);
	CHECK(found[1].value().facts.anilistId == 30002);
	CHECK(found[2].value().facts.totalUnits == 13);
	CHECK(found[3].errorMessage() == "the title has no id to look up");
	// One request for each list, a pause between them.
	CHECK(web.sent.size() == 2);
	CHECK(pauses == 1);
}

TEST_CASE("anilist many-title lookups fail as one when AniList cannot be reached") {
	catalist::test::fakeWeb web;
	anilist::tracker site(web.link());
	auto found = site.lookUpMany({catalist::test::makeEntry().anilistId(1),
	                              catalist::test::makeEntry().type(mediaType::kManga).anilistId(2)});
	REQUIRE(found.size() == 2);
	CHECK(found[0].errorMessage() == "could not reach AniList");
	CHECK(found[1].errorMessage() == "could not reach AniList");
	// Once it is not there, it is not asked again.
	CHECK(web.sent.size() == 1);
}
