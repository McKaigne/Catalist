#include <doctest/doctest.h>

#include "support/testSupport.hpp"
#include "web/anilist.hpp"
#include "web/json.hpp"
#include "web/kitsu.hpp"
#include "web/mal.hpp"

using catalist::mediaType;
using catalist::watchStatus;

TEST_CASE("anilist reads a user's list, with the ids and the person's fields") {
	auto request = catalist::anilist::listRequest(mediaType::kAnime, "Abel");
	CHECK(catalist::json::parse(request).ok());
	CHECK(request.find(R"("u":"Abel","t":"ANIME")") != std::string::npos);
	auto found = catalist::anilist::parseList(
	    R"({"data":{"MediaListCollection":{"lists":[{"entries":[
	    {"status":"REPEATING","score":8.0,"progress":5,"progressVolumes":0,"repeat":1,
	     "startedAt":{"year":2026,"month":10,"day":1},"completedAt":{"year":null,"month":null,"day":null},
	     "media":{"id":128547,"idMal":44265,"title":{"romaji":"Odd Taxi"},"episodes":13,"status":"FINISHED"}},
	    {"status":"CUSTOM","media":{"id":1,"title":{"romaji":"x"}}}]},
	    {"entries":[{"status":"PLANNING","score":0,"progress":0,"repeat":0,"startedAt":{},"completedAt":{},
	     "media":{"id":154587,"idMal":52991,"title":{"romaji":"Sousou no Frieren"},"episodes":28,"status":"FINISHED"}}]}]}}})",
	    mediaType::kAnime);
	REQUIRE(found.ok());
	REQUIRE(found.value().size() == 2);
	const auto& taxi = found.value()[0];
	CHECK(taxi.title == "Odd Taxi");
	CHECK(taxi.status == watchStatus::kWatching);
	CHECK(taxi.score == 8);
	CHECK(taxi.progress == 5);
	CHECK(taxi.rewatched == 1);
	CHECK(taxi.anilistId == 128547);
	CHECK(taxi.malId == 44265);
	CHECK(taxi.released == 13);
	CHECK(catalist::formatDate(taxi.startDate) == "2026-10-01");
	CHECK_FALSE(taxi.finishDate);
	CHECK(found.value()[1].status == watchStatus::kPlanToWatch);
	CHECK_FALSE(catalist::anilist::parseList("{}", mediaType::kAnime).ok());
}

TEST_CASE("kitsu reads a user's library page by page") {
	catalist::test::fakeWeb web;
	web.respond("GET https://kitsu.app/api/edge/users?filter[name]=abel",
	            R"({"data":[{"id":"77"}]})");
	web.respond(
	    "GET https://kitsu.app/api/edge/library-entries?filter[userId]=77&filter[kind]=anime",
	    R"({"data":[{"id":"1","attributes":{"status":"completed","progress":28,
	            "reconsumeCount":0,"ratingTwenty":19,"finishedAt":"2024-03-22T00:00:00.000Z"},
	            "relationships":{"anime":{"data":{"type":"anime","id":"46474"}}}}],
	            "included":[{"id":"46474","type":"anime","attributes":{"canonicalTitle":
	            "Sousou no Frieren","episodeCount":28,"status":"finished"},
	            "relationships":{"mappings":{"data":[{"type":"mappings","id":"7"}]}}},
	            {"id":"7","type":"mappings","attributes":{"externalSite":"myanimelist/anime",
	            "externalId":"52991"}}],
	            "links":{"next":"https://kitsu.app/api/edge/library-entries?page2"}})");
	web.respond("GET https://kitsu.app/api/edge/library-entries?page2",
	            R"({"data":[{"id":"2","attributes":{"status":"current","progress":5},
	            "relationships":{"anime":{"data":{"type":"anime","id":"43095"}}}}],
	            "included":[{"id":"43095","type":"anime","attributes":{"canonicalTitle":"Odd Taxi"}}],
	            "links":{}})");
	catalist::kitsu::tracker site(web.link());
	auto found = site.pull(mediaType::kAnime, "abel");
	REQUIRE(found.ok());
	REQUIRE(found.value().size() == 2);
	const auto& frieren = found.value()[0];
	CHECK(frieren.kitsuId == 46474);
	CHECK(frieren.malId == 52991);
	CHECK(frieren.score == 10);
	CHECK(frieren.status == watchStatus::kCompleted);
	CHECK(frieren.released == 28);
	CHECK(catalist::formatDate(frieren.finishDate) == "2024-03-22");
	CHECK(found.value()[1].title == "Odd Taxi");
	CHECK(found.value()[1].score == 0);

	web.respond("filter[name]=nobody", R"({"data":[]})");
	CHECK(site.pull(mediaType::kAnime, "nobody").errorMessage() ==
	      "Kitsu has no user named nobody");
}

TEST_CASE("mal reads a user's list page by page") {
	CHECK(catalist::mal::listUrl(mediaType::kManga, "Abel") ==
	      "https://api.myanimelist.net/v2/users/Abel/mangalist?fields=list_status{status,score,"
	      "num_chapters_read,num_volumes_read,num_times_reread,start_date,finish_date},"
	      "num_chapters,num_volumes,status&limit=1000&nsfw=true");
	std::string next;
	auto found = catalist::mal::parseList(
	    R"({"data":[{"node":{"id":2,"title":"Berserk","num_chapters":0,"status":"currently_publishing"},
	    "list_status":{"status":"reading","score":10,"num_chapters_read":364,"num_volumes_read":41,
	    "num_times_reread":0,"start_date":"2019-05-01"}},
	    {"node":{"id":3,"title":"x"},"list_status":{"status":"unknown"}}],
	    "paging":{"next":"https://api.myanimelist.net/v2/users/Abel/mangalist?offset=1000"}})",
	    mediaType::kManga, next);
	REQUIRE(found.ok());
	REQUIRE(found.value().size() == 1);
	CHECK(found.value()[0].progress == 364);
	CHECK(found.value()[0].volumes == 41);
	CHECK(found.value()[0].score == 10);
	CHECK(found.value()[0].released == 0);
	CHECK(next == "https://api.myanimelist.net/v2/users/Abel/mangalist?offset=1000");
	CHECK(catalist::mal::parseList(R"({"data":[],"paging":{}})", mediaType::kManga, next).ok());
	CHECK(next.empty());
}
