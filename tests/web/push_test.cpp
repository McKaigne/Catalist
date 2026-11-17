#include <doctest/doctest.h>

#include "support/test_support.hpp"
#include "web/anilist.hpp"
#include "web/json.hpp"
#include "web/kitsu.hpp"
#include "web/mal.hpp"

using catalist::Account;
using catalist::MediaType;
using catalist::Provider;
using catalist::WatchStatus;
using catalist::test::MakeEntry;

namespace {

const Account kLogin = {Provider::kAniList, "Abel", "77", "tok", "", 0};

catalist::Entry OddTaxi() {
	return MakeEntry()
	    .Title("Odd Taxi")
	    .AnilistId(128547)
	    .MalId(44265)
	    .KitsuId(43095)
	    .Status(WatchStatus::kWatching)
	    .Score(8)
	    .Progress(5)
	    .Rewatched(1)
	    .StartDate("2026-10-01");
}

}  // namespace

TEST_CASE("anilist sets the whole list entry, the score out of 100") {
	std::string request = catalist::anilist::SaveRequest(OddTaxi());
	CHECK(catalist::json::Parse(request).has_value());
	CHECK(request.find(R"("m":128547,"s":"CURRENT","c":80,"p":5,"v":0,"r":1,)") !=
	      std::string::npos);
	CHECK(request.find(R"("a":{"year":2026,"month":10,"day":1})") != std::string::npos);
	// A date the title has not is cleared there too.
	CHECK(request.find(R"("f":{"year":null,"month":null,"day":null})") != std::string::npos);

	catalist::test::FakeWeb web;
	web.Respond("SaveMediaListEntry", R"({"data":{"SaveMediaListEntry":{"id":9}}})");
	catalist::anilist::Tracker site(web.Link());
	CHECK(site.Push(OddTaxi(), kLogin, {}).has_value());
	CHECK(web.sent.size() == 1);
	catalist::Entry unlinked = OddTaxi();
	unlinked.anilist_id = 0;
	CHECK(site.Push(unlinked, kLogin, {}).error().message == "not linked to AniList");
	CHECK(site.Push(unlinked, kLogin, {}).error().why == catalist::web::Failure::Kind::kNotLinked);
}

TEST_CASE("anilist removes a title by its list entry, and is done when there is none") {
	catalist::test::FakeWeb web;
	web.Respond("MediaList(mediaId:$m,userId:$u)", R"({"data":{"MediaList":{"id":31}}})");
	web.Respond("DeleteMediaListEntry", R"({"data":{"DeleteMediaListEntry":{"deleted":true}}})");
	catalist::anilist::Tracker site(web.Link());
	CHECK(site.Remove(MediaType::kAnime, 128547, kLogin).has_value());
	REQUIRE(web.sent.size() == 2);
	CHECK(web.sent[1].find(R"("i":31)") != std::string::npos);

	// AniList answers 404 for a title not on the list: gone already.
	catalist::test::FakeWeb none;
	none.Respond("MediaList(mediaId:$m,userId:$u)", R"({"data":{"MediaList":null}})", 404);
	catalist::anilist::Tracker empty(none.Link());
	CHECK(empty.Remove(MediaType::kAnime, 1, kLogin).has_value());
	// Not reached: that is no answer.
	catalist::test::FakeWeb offline;
	catalist::anilist::Tracker away(offline.Link());
	CHECK(away.Remove(MediaType::kAnime, 1, kLogin).error().why ==
	      catalist::web::Failure::Kind::kUnreachable);
}

TEST_CASE("mal sets the list status with the fields of the list") {
	CHECK(catalist::mal::ListStatusForm(OddTaxi()) ==
	      "status=watching&score=8&is_rewatching=false&num_watched_episodes=5&num_times_rewatched=1"
	      "&start_date=2026-10-01");
	catalist::Entry berserk = MakeEntry()
	                              .Type(MediaType::kManga)
	                              .MalId(2)
	                              .Status(WatchStatus::kPlanToWatch)
	                              .Progress(364)
	                              .Volumes(41);
	CHECK(catalist::mal::ListStatusForm(berserk) ==
	      "status=plan_to_read&score=0&is_rereading=false&num_chapters_read=364&num_volumes_read=41"
	      "&num_times_reread=0");

	catalist::test::FakeWeb web;
	web.Respond("PATCH https://api.myanimelist.net/v2/anime/44265/my_list_status",
	            R"({"status":"watching"})");
	auto net = web.Link();
	net.ids.mal = "mal-id";
	catalist::mal::Tracker site(net);
	CHECK(site.Push(OddTaxi(), kLogin, {}).has_value());
	CHECK_FALSE(site.Remove(MediaType::kAnime, 44265, kLogin).has_value());
}

TEST_CASE("kitsu changes the library entry it has, or makes one") {
	CHECK(
	    catalist::kitsu::LibraryAttributes(OddTaxi()) ==
	    R"({"status":"current","reconsuming":false,"progress":5,"reconsumeCount":1,"ratingTwenty":16,)"
	    R"("startedAt":"2026-10-01T00:00:00.000Z","finishedAt":null})");

	catalist::test::FakeWeb web;
	web.Respond(
	    "GET https://kitsu.app/api/edge/library-entries?filter[userId]=77"
	    "&filter[animeId]=43095",
	    R"({"data":[{"id":"555"}]})");
	web.Respond("PATCH https://kitsu.app/api/edge/library-entries/555", "{}");
	catalist::kitsu::Tracker site(web.Link());
	CHECK(site.Push(OddTaxi(), kLogin, {}).has_value());
	REQUIRE(web.sent.size() == 2);
	CHECK(web.sent[1].find(R"({"data":{"id":"555","type":"libraryEntries","attributes":)") !=
	      std::string::npos);

	catalist::test::FakeWeb fresh;
	fresh.Respond("filter[animeId]=43095", R"({"data":[]})");
	fresh.Respond("POST https://kitsu.app/api/edge/library-entries", "{}");
	catalist::kitsu::Tracker making(fresh.Link());
	CHECK(making.Push(OddTaxi(), kLogin, {}).has_value());
	REQUIRE(fresh.sent.size() == 2);
	CHECK(fresh.sent[1].find(R"("user":{"data":{"type":"users","id":"77"}},)"
	                         R"("anime":{"data":{"type":"anime","id":"43095"}})") !=
	      std::string::npos);
	// Not on the list: removed already.
	CHECK(making.Remove(MediaType::kAnime, 43095, kLogin).has_value());
}

TEST_CASE("a push sends only the fields it is given") {
	namespace field = catalist::site_field;
	auto taxi = OddTaxi();
	CHECK(catalist::anilist::SaveRequest(taxi, {field::kProgress, false}) ==
	      R"({"query":"mutation($m:Int,$p:Int){SaveMediaListEntry(mediaId:$m,progress:$p){id}}",)"
	      R"("variables":{"m":128547,"p":5}})");
	auto hidden = catalist::anilist::SaveRequest(taxi, {field::kScore | field::kHidden, true});
	CHECK(hidden.find(R"("variables":{"m":128547,"c":80,"h":true})") != std::string::npos);
	CHECK(catalist::json::Parse(hidden).has_value());
	CHECK(catalist::mal::ListStatusForm(taxi, field::kStatus | field::kProgress) ==
	      "status=watching&is_rewatching=false&num_watched_episodes=5");
	CHECK(catalist::kitsu::LibraryAttributes(taxi, {field::kScore | field::kHidden, true}) ==
	      R"({"ratingTwenty":16,"private":true})");
}
