#include <doctest/doctest.h>

#include "support/testSupport.hpp"
#include "web/anilist.hpp"
#include "web/kitsu.hpp"
#include "web/mal.hpp"

using catalist::mediaType;
using catalist::watchStatus;
using catalist::test::makeEntry;

namespace {

const catalist::account kLogin{catalist::provider::kAniList, "Abel", "77", "tok", "", 0};

}  // namespace

TEST_CASE("anilist reads your entry of a title, or says it is not on your list") {
	catalist::test::fakeWeb web;
	web.respond("mediaList(userId:$u,mediaId:$m)", R"({"data":{"Page":{"mediaList":[
	  {"status":"REPEATING","score":85,"progress":4,"progressVolumes":0,"repeat":1,"private":true,
	   "startedAt":{"year":2024,"month":3,"day":1},"completedAt":{"year":null}}]}}})");
	catalist::anilist::tracker site(web.link());
	auto found = site.entryOf(makeEntry().anilistId(154587), kLogin);
	REQUIRE(found.has_value());
	REQUIRE(found.value().has_value());
	const auto& mine = *found.value();
	CHECK(mine.status == watchStatus::kRewatching);
	CHECK(mine.score == 85);
	CHECK(catalist::web::outOfTen(catalist::provider::kAniList, mine.score) == doctest::Approx(8.5));
	CHECK(mine.progress == 4);
	CHECK(mine.rewatched == 1);
	CHECK(mine.hidden);
	CHECK(catalist::formatDate(mine.startDate) == "2024-03-01");
	CHECK_FALSE(mine.finishDate);
	CHECK(web.sent[0].find(R"("m":154587,"u":77)") != std::string::npos);

	web.respond("mediaList(userId:$u,mediaId:$m)", R"({"data":{"Page":{"mediaList":[]}}})");
	auto none = site.entryOf(makeEntry().anilistId(154587), kLogin);
	REQUIRE(none.has_value());
	CHECK_FALSE(none.value().has_value());
	CHECK(site.entryOf(makeEntry(), kLogin).error().why ==
	      catalist::web::failure::kind::kNotLinked);
}

TEST_CASE("mal reads your entry of a title from its my_list_status") {
	auto found = catalist::mal::parseEntry(R"({"id":2,"title":"Berserk","my_list_status":{
	  "status":"reading","score":10,"num_chapters_read":364,"num_volumes_read":41,
	  "is_rereading":false,"num_times_reread":0,"start_date":"2020-01-02"}})",
	                                       mediaType::kManga);
	REQUIRE(found.has_value());
	REQUIRE(found.value().has_value());
	CHECK(found.value()->status == watchStatus::kWatching);
	CHECK(found.value()->score == 10);
	CHECK(found.value()->progress == 364);
	CHECK(found.value()->volumes == 41);
	CHECK(catalist::formatDate(found.value()->startDate) == "2020-01-02");

	auto again = catalist::mal::parseEntry(
	    R"({"id":1,"my_list_status":{"status":"completed","is_rewatching":true}})",
	    mediaType::kAnime);
	CHECK(again.value()->status == watchStatus::kRewatching);
	auto none = catalist::mal::parseEntry(R"({"id":1,"title":"Cowboy Bebop"})", mediaType::kAnime);
	REQUIRE(none.has_value());
	CHECK_FALSE(none.value().has_value());
	CHECK(catalist::mal::entryUrl(mediaType::kAnime, 1) ==
	      "https://api.myanimelist.net/v2/anime/1?fields=my_list_status{status,score,"
	      "num_episodes_watched,is_rewatching,num_times_rewatched,start_date,finish_date}");
}

TEST_CASE("kitsu reads your library entry of a title") {
	auto found = catalist::kitsu::parseEntry(R"({"data":[{"id":"9","attributes":{
	  "status":"current","reconsuming":false,"progress":5,"reconsumeCount":0,"ratingTwenty":17,
	  "private":true,"startedAt":"2026-10-01T00:00:00.000Z","finishedAt":null}}]})");
	REQUIRE(found.has_value());
	REQUIRE(found.value().has_value());
	CHECK(found.value()->status == watchStatus::kWatching);
	CHECK(found.value()->score == 17);
	CHECK(catalist::web::outOfTen(catalist::provider::kKitsu, 17) == doctest::Approx(8.5));
	CHECK(found.value()->hidden);
	CHECK(catalist::formatDate(found.value()->startDate) == "2026-10-01");
	auto none = catalist::kitsu::parseEntry(R"({"data":[]})");
	REQUIRE(none.has_value());
	CHECK_FALSE(none.value().has_value());
}
