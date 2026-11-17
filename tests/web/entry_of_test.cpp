#include <doctest/doctest.h>

#include "support/test_support.hpp"
#include "web/anilist.hpp"
#include "web/kitsu.hpp"
#include "web/mal.hpp"

using catalist::MediaType;
using catalist::WatchStatus;
using catalist::test::MakeEntry;

namespace {

const catalist::Account kLogin{catalist::Provider::kAniList, "Abel", "77", "tok", "", 0};

}  // namespace

TEST_CASE("anilist reads your entry of a title, or says it is not on your list") {
	catalist::test::FakeWeb web;
	web.Respond("mediaList(userId:$u,mediaId:$m)", R"({"data":{"Page":{"mediaList":[
	  {"status":"REPEATING","score":85,"progress":4,"progressVolumes":0,"repeat":1,"private":true,
	   "startedAt":{"year":2024,"month":3,"day":1},"completedAt":{"year":null}}]}}})");
	catalist::anilist::Tracker site(web.Link());
	auto found = site.EntryOf(MakeEntry().AnilistId(154587), kLogin);
	REQUIRE(found.has_value());
	REQUIRE(found.value().has_value());
	const auto& mine = *found.value();
	CHECK(mine.status == WatchStatus::kRewatching);
	CHECK(mine.score == 85);
	CHECK(catalist::web::OutOfTen(catalist::Provider::kAniList, mine.score) ==
	      doctest::Approx(8.5));
	CHECK(mine.progress == 4);
	CHECK(mine.rewatched == 1);
	CHECK(mine.hidden);
	CHECK(catalist::FormatDate(mine.start_date) == "2024-03-01");
	CHECK_FALSE(mine.finish_date);
	CHECK(web.sent[0].find(R"("m":154587,"u":77)") != std::string::npos);

	web.Respond("mediaList(userId:$u,mediaId:$m)", R"({"data":{"Page":{"mediaList":[]}}})");
	auto none = site.EntryOf(MakeEntry().AnilistId(154587), kLogin);
	REQUIRE(none.has_value());
	CHECK_FALSE(none.value().has_value());
	CHECK(site.EntryOf(MakeEntry(), kLogin).error().why ==
	      catalist::web::Failure::Kind::kNotLinked);
}

TEST_CASE("mal reads your entry of a title from its my_list_status") {
	auto found = catalist::mal::ParseEntry(R"({"id":2,"title":"Berserk","my_list_status":{
	  "status":"reading","score":10,"num_chapters_read":364,"num_volumes_read":41,
	  "is_rereading":false,"num_times_reread":0,"start_date":"2020-01-02"}})",
	                                       MediaType::kManga);
	REQUIRE(found.has_value());
	REQUIRE(found.value().has_value());
	CHECK(found.value()->status == WatchStatus::kWatching);
	CHECK(found.value()->score == 10);
	CHECK(found.value()->progress == 364);
	CHECK(found.value()->volumes == 41);
	CHECK(catalist::FormatDate(found.value()->start_date) == "2020-01-02");

	auto again = catalist::mal::ParseEntry(
	    R"({"id":1,"my_list_status":{"status":"completed","is_rewatching":true}})",
	    MediaType::kAnime);
	CHECK(again.value()->status == WatchStatus::kRewatching);
	auto none = catalist::mal::ParseEntry(R"({"id":1,"title":"Cowboy Bebop"})", MediaType::kAnime);
	REQUIRE(none.has_value());
	CHECK_FALSE(none.value().has_value());
	CHECK(catalist::mal::EntryUrl(MediaType::kAnime, 1) ==
	      "https://api.myanimelist.net/v2/anime/1?fields=my_list_status{status,score,"
	      "num_episodes_watched,is_rewatching,num_times_rewatched,start_date,finish_date}");
}

TEST_CASE("kitsu reads your library entry of a title") {
	auto found = catalist::kitsu::ParseEntry(R"({"data":[{"id":"9","attributes":{
	  "status":"current","reconsuming":false,"progress":5,"reconsumeCount":0,"ratingTwenty":17,
	  "private":true,"startedAt":"2026-10-01T00:00:00.000Z","finishedAt":null}}]})");
	REQUIRE(found.has_value());
	REQUIRE(found.value().has_value());
	CHECK(found.value()->status == WatchStatus::kWatching);
	CHECK(found.value()->score == 17);
	CHECK(catalist::web::OutOfTen(catalist::Provider::kKitsu, 17) == doctest::Approx(8.5));
	CHECK(found.value()->hidden);
	CHECK(catalist::FormatDate(found.value()->start_date) == "2026-10-01");
	auto none = catalist::kitsu::ParseEntry(R"({"data":[]})");
	REQUIRE(none.has_value());
	CHECK_FALSE(none.value().has_value());
}
