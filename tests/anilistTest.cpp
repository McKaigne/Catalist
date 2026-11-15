#include <doctest/doctest.h>

#include "anilist.hpp"
#include "json.hpp"
#include "support/testSupport.hpp"
#include "updater.hpp"

namespace anilist = catalist::anilist;
using catalist::mediaType;

namespace {

// Real replies, shortened: Frieren (finished), One Piece (airing) and
// Berserk (a manga still running).
const char* kFrieren =
    R"({"data":{"Media":{"title":{"romaji":"Sousou no Frieren"},"episodes":28,"chapters":null,"volumes":null,
  "status":"FINISHED","coverImage":{"large":"https:\/\/s4.anilist.co\/m\/bx154587.jpg",
  "extraLarge":"https:\/\/s4.anilist.co\/l\/bx154587.jpg"},"nextAiringEpisode":null}}})";
const char* kOnePiece = R"({"data":{"Media":{"episodes":null,"chapters":null,"volumes":null,
  "status":"RELEASING","coverImage":{"large":"https:\/\/s4.anilist.co\/m\/bx21.jpg"},
  "nextAiringEpisode":{"episode":1181}}}})";
const char* kBerserk = R"({"data":{"Media":{"episodes":null,"chapters":null,"volumes":null,
  "status":"RELEASING","coverImage":{"large":"https:\/\/s4.anilist.co\/m\/bx30002.jpg"}}}})";

}  // namespace

TEST_CASE("anilist reads a finished title, its totals and its larger cover") {
	auto found = anilist::parseLookup(kFrieren, mediaType::kAnime);
	REQUIRE(found.ok());
	CHECK(found.value().name == "Sousou no Frieren");
	CHECK(found.value().facts.totalUnits == 28);
	CHECK(found.value().facts.released == 28);
	CHECK_FALSE(found.value().ongoing);
	CHECK(found.value().imageUrl == "https://s4.anilist.co/l/bx154587.jpg");
}

TEST_CASE("anilist counts the aired episodes from the next one to air") {
	auto found = anilist::parseLookup(kOnePiece, mediaType::kAnime);
	REQUIRE(found.ok());
	CHECK(found.value().ongoing);
	CHECK(found.value().facts.totalUnits == 0);
	CHECK(found.value().facts.released == 1180);
	CHECK(found.value().imageUrl == "https://s4.anilist.co/m/bx21.jpg");
}

TEST_CASE("anilist leaves a running manga's chapters unknown") {
	auto found = anilist::parseLookup(kBerserk, mediaType::kManga);
	REQUIRE(found.ok());
	CHECK(found.value().ongoing);
	CHECK(found.value().facts.released == 0);
	CHECK_FALSE(anilist::parseLookup(R"({"data":{"Media":null}})", mediaType::kManga).ok());
	CHECK_FALSE(anilist::parseLookup("<html>", mediaType::kManga).ok());
}

TEST_CASE("anilist requests ask by MyAnimeList id and escape the search") {
	CHECK(anilist::lookupRequest(mediaType::kManga, 2)
	          .find(R"("variables":{"id":2,"type":"MANGA"})") != std::string::npos);
	CHECK(anilist::lookupRequest(mediaType::kAnime, 52991).find("idMal:$id") != std::string::npos);
	auto search = anilist::searchRequest(mediaType::kAnime, "say \"hi\"\\\n");
	CHECK(search.find(R"("search":"say \"hi\"\\\u000a")") != std::string::npos);
	CHECK(catalist::json::parse(search).ok());
	CHECK(catalist::json::parse(anilist::lookupRequest(mediaType::kAnime, 1)).ok());
}

TEST_CASE("anilist search keeps the matches that have a MyAnimeList id") {
	auto found = anilist::parseSearch(
	    R"({"data":{"Page":{"media":[
	      {"idMal":5114,"title":{"romaji":"Fullmetal Alchemist: Brotherhood"},"episodes":64,
	       "averageScore":90,"startDate":{"year":2009}},
	      {"idMal":null,"title":{"romaji":"AniList only"},"episodes":3},
	      {"idMal":121,"title":{"romaji":"Fullmetal Alchemist"},"episodes":51,
	       "averageScore":null,"startDate":{"year":null}}]}}})",
	    mediaType::kAnime);
	REQUIRE(found.ok());
	REQUIRE(found.value().size() == 2);
	CHECK(found.value()[0].malId == 5114);
	CHECK(found.value()[0].totalUnits == 64);
	CHECK(found.value()[0].year == "2009");
	CHECK(found.value()[0].score == doctest::Approx(9.0));
	CHECK(found.value()[1].year.empty());
	CHECK(found.value()[1].score == 0.0);
}

TEST_CASE("updater asks AniList first and Jikan only when AniList fails") {
	catalist::test::fakeWeb web;
	web.respond(R"("id":9,"type":"ANIME")", kOnePiece);
	catalist::updater updates(
	    web.fetcher(), web.poster(), catalist::test::testToday,
	    [](const std::string&, int, int) { return std::optional<std::string>(); },
	    std::chrono::milliseconds(0));
	updates.lookUp(mediaType::kAnime, 9);
	updates.waitUntilIdle();
	auto findings = updates.takeFindings();
	REQUIRE(findings.size() == 1);
	CHECK(findings[0].facts.released == 1180);
	CHECK(web.requested.empty());

	// AniList does not know title 10, so Jikan is asked.
	web.respond("/anime/10", R"({"data": {"status": "Finished Airing", "episodes": 12}})");
	updates.lookUp(mediaType::kAnime, 10);
	updates.waitUntilIdle();
	findings = updates.takeFindings();
	REQUIRE(findings.size() == 1);
	CHECK(findings[0].facts.totalUnits == 12);
	CHECK(web.requested == std::vector<std::string>{"https://api.jikan.moe/v4/anime/10"});
}

TEST_CASE("updater takes covers from AniList") {
	catalist::test::fakeWeb web;
	web.respond(R"("id":52991,"type":"ANIME")", kFrieren);
	web.respond("s4.anilist.co/l/bx154587.jpg", "JPEG");
	catalist::updater updates(
	    web.fetcher(), web.poster(), catalist::test::testToday,
	    [](const std::string& image, int, int) { return std::optional<std::string>(image); },
	    std::chrono::milliseconds(0));
	CHECK(updates.coverFor(mediaType::kAnime, 52991, 70, 100).state ==
	      catalist::updater::coverState::kLoading);
	updates.waitUntilIdle();
	auto cover = updates.coverFor(mediaType::kAnime, 52991, 70, 100);
	CHECK(cover.state == catalist::updater::coverState::kReady);
	CHECK(cover.sixel == "JPEG");
}
