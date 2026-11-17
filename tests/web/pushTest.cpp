#include <doctest/doctest.h>

#include "support/testSupport.hpp"
#include "web/anilist.hpp"
#include "web/json.hpp"
#include "web/kitsu.hpp"
#include "web/mal.hpp"

using catalist::account;
using catalist::mediaType;
using catalist::provider;
using catalist::watchStatus;
using catalist::test::makeEntry;

namespace {

const account kLogin = {provider::kAniList, "Abel", "77", "tok", "", 0};

catalist::entry oddTaxi() {
	return makeEntry()
	    .title("Odd Taxi")
	    .anilistId(128547)
	    .malId(44265)
	    .kitsuId(43095)
	    .status(watchStatus::kWatching)
	    .score(8)
	    .progress(5)
	    .rewatched(1)
	    .startDate("2026-10-01");
}

}  // namespace

TEST_CASE("anilist sets the whole list entry, the score out of 100") {
	std::string request = catalist::anilist::saveRequest(oddTaxi());
	CHECK(catalist::json::parse(request).has_value());
	CHECK(request.find(R"("m":128547,"s":"CURRENT","c":80,"p":5,"v":0,"r":1,)") !=
	      std::string::npos);
	CHECK(request.find(R"("a":{"year":2026,"month":10,"day":1})") != std::string::npos);
	// A date the title has not is cleared there too.
	CHECK(request.find(R"("f":{"year":null,"month":null,"day":null})") != std::string::npos);

	catalist::test::fakeWeb web;
	web.respond("SaveMediaListEntry", R"({"data":{"SaveMediaListEntry":{"id":9}}})");
	catalist::anilist::tracker site(web.link());
	CHECK(site.push(oddTaxi(), kLogin, {}).has_value());
	CHECK(web.sent.size() == 1);
	catalist::entry unlinked = oddTaxi();
	unlinked.anilistId = 0;
	CHECK(site.push(unlinked, kLogin, {}).error().message == "not linked to AniList");
	CHECK(site.push(unlinked, kLogin, {}).error().why == catalist::web::failure::kind::kNotLinked);
}

TEST_CASE("anilist removes a title by its list entry, and is done when there is none") {
	catalist::test::fakeWeb web;
	web.respond("MediaList(mediaId:$m,userId:$u)", R"({"data":{"MediaList":{"id":31}}})");
	web.respond("DeleteMediaListEntry", R"({"data":{"DeleteMediaListEntry":{"deleted":true}}})");
	catalist::anilist::tracker site(web.link());
	CHECK(site.remove(mediaType::kAnime, 128547, kLogin).has_value());
	REQUIRE(web.sent.size() == 2);
	CHECK(web.sent[1].find(R"("i":31)") != std::string::npos);

	// AniList answers 404 for a title not on the list: gone already.
	catalist::test::fakeWeb none;
	none.respond("MediaList(mediaId:$m,userId:$u)", R"({"data":{"MediaList":null}})", 404);
	catalist::anilist::tracker empty(none.link());
	CHECK(empty.remove(mediaType::kAnime, 1, kLogin).has_value());
	// Not reached: that is no answer.
	catalist::test::fakeWeb offline;
	catalist::anilist::tracker away(offline.link());
	CHECK(away.remove(mediaType::kAnime, 1, kLogin).error().why ==
	      catalist::web::failure::kind::kUnreachable);
}

TEST_CASE("mal sets the list status with the fields of the list") {
	CHECK(catalist::mal::listStatusForm(oddTaxi()) ==
	      "status=watching&score=8&is_rewatching=false&num_watched_episodes=5&num_times_rewatched=1"
	      "&start_date=2026-10-01");
	catalist::entry berserk = makeEntry()
	                              .type(mediaType::kManga)
	                              .malId(2)
	                              .status(watchStatus::kPlanToWatch)
	                              .progress(364)
	                              .volumes(41);
	CHECK(catalist::mal::listStatusForm(berserk) ==
	      "status=plan_to_read&score=0&is_rereading=false&num_chapters_read=364&num_volumes_read=41"
	      "&num_times_reread=0");

	catalist::test::fakeWeb web;
	web.respond("PATCH https://api.myanimelist.net/v2/anime/44265/my_list_status",
	            R"({"status":"watching"})");
	auto net = web.link();
	net.ids.mal = "mal-id";
	catalist::mal::tracker site(net);
	CHECK(site.push(oddTaxi(), kLogin, {}).has_value());
	CHECK_FALSE(site.remove(mediaType::kAnime, 44265, kLogin).has_value());
}

TEST_CASE("kitsu changes the library entry it has, or makes one") {
	CHECK(catalist::kitsu::libraryAttributes(oddTaxi()) ==
	      R"({"status":"current","reconsuming":false,"progress":5,"reconsumeCount":1,"ratingTwenty":16,)"
	      R"("startedAt":"2026-10-01T00:00:00.000Z","finishedAt":null})");

	catalist::test::fakeWeb web;
	web.respond(
	    "GET https://kitsu.app/api/edge/library-entries?filter[userId]=77"
	    "&filter[animeId]=43095",
	    R"({"data":[{"id":"555"}]})");
	web.respond("PATCH https://kitsu.app/api/edge/library-entries/555", "{}");
	catalist::kitsu::tracker site(web.link());
	CHECK(site.push(oddTaxi(), kLogin, {}).has_value());
	REQUIRE(web.sent.size() == 2);
	CHECK(web.sent[1].find(R"({"data":{"id":"555","type":"libraryEntries","attributes":)") !=
	      std::string::npos);

	catalist::test::fakeWeb fresh;
	fresh.respond("filter[animeId]=43095", R"({"data":[]})");
	fresh.respond("POST https://kitsu.app/api/edge/library-entries", "{}");
	catalist::kitsu::tracker making(fresh.link());
	CHECK(making.push(oddTaxi(), kLogin, {}).has_value());
	REQUIRE(fresh.sent.size() == 2);
	CHECK(fresh.sent[1].find(R"("user":{"data":{"type":"users","id":"77"}},)"
	                         R"("anime":{"data":{"type":"anime","id":"43095"}})") !=
	      std::string::npos);
	// Not on the list: removed already.
	CHECK(making.remove(mediaType::kAnime, 43095, kLogin).has_value());
}

TEST_CASE("a push sends only the fields it is given") {
	namespace field = catalist::siteField;
	auto taxi = oddTaxi();
	CHECK(catalist::anilist::saveRequest(taxi, {field::kProgress, false}) ==
	      R"({"query":"mutation($m:Int,$p:Int){SaveMediaListEntry(mediaId:$m,progress:$p){id}}",)"
	      R"("variables":{"m":128547,"p":5}})");
	auto hidden = catalist::anilist::saveRequest(taxi, {field::kScore | field::kHidden, true});
	CHECK(hidden.find(R"("variables":{"m":128547,"c":80,"h":true})") != std::string::npos);
	CHECK(catalist::json::parse(hidden).has_value());
	CHECK(catalist::mal::listStatusForm(taxi, field::kStatus | field::kProgress) ==
	      "status=watching&is_rewatching=false&num_watched_episodes=5");
	CHECK(catalist::kitsu::libraryAttributes(taxi, {field::kScore | field::kHidden, true}) ==
	      R"({"ratingTwenty":16,"private":true})");
}
