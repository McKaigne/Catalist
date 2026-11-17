#include <doctest/doctest.h>

#include "support/cli_fixture.hpp"

using catalist::MediaType;
using catalist::Provider;
using catalist::WatchStatus;
using catalist::test::CliFixture;
using catalist::test::Has;

namespace {

const char* kSaved = R"({"data":{"SaveMediaListEntry":{"id":9}}})";
const char* kEntry = "mediaList(userId:$u,mediaId:$m)";
// Odd Taxi on AniList: further along, scored 85 of 100, hidden.
const char* kAhead = R"({"data":{"Page":{"mediaList":[{"status":"CURRENT","score":85,
  "progress":9,"repeat":0,"private":true,"startedAt":{"year":2026,"month":10,"day":1},
  "completedAt":{}}]}}})";

}  // namespace

TEST_CASE_FIXTURE(CliFixture, "cli track shows a title on each site beside the lists") {
	Seed();
	LogIn(Provider::kAniList);
	web.Respond(kEntry, kAhead);
	CHECK(Run({"track", "taxi"}) == 0);
	CHECK(out ==
	      "anilist\t128547\tlisted\twatching\t85\t9\t0\t0\t2026-10-01\t-\ttrue\tscore,progress\t"
	      "https://anilist.co/anime/128547\n"
	      "mal\t44265\tlogged-out\t-\t-\t-\t-\t-\t-\t-\t-\t-\thttps://myanimelist.net/anime/44265\n"
	      "kitsu\t0\tunlinked\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\n");
	CHECK(Run({"track", "--json", "taxi"}) == 0);
	CHECK(Has(out,
	          R"({"site":"anilist","id":128547,"state":"listed","status":"watching","score":85,)"));
	CHECK(Has(out, R"("hidden":true,"differs":"score,progress",)"));

	// At a terminal, with what to do about it.
	CHECK(Run({"track", "taxi"}, "", 120) == 0);
	CHECK(Has(out, "Odd Taxi\nSITE "));
	CHECK(Has(out, "AniList      128547  Watching"));
	CHECK(Has(out, "  9/13      2026-10-01  -         hidden\n"));
	CHECK(Has(out, "MyAnimeList  44265   not logged in\n"));
	CHECK(Has(out, "Kitsu        -       not linked\n"));
	CHECK(Has(err, "'catalist track --pull anilist a:2' takes AniList's as the title's"));

	// Not on your list there, or not reached.
	web.Respond(kEntry, R"({"data":{"Page":{"mediaList":[]}}})");
	CHECK(Run({"track", "taxi"}) == 0);
	CHECK(Has(out, "anilist\t128547\tmissing\t"));
	web.bodies.erase(kEntry);
	CHECK(Run({"track", "taxi"}) == 0);
	CHECK(Has(out, "anilist\t128547\tfailed\t"));
	CHECK(Run({"track", "taxi", "frieren"}) == 2);
}

TEST_CASE_FIXTURE(CliFixture, "cli track --private hides a title on a site") {
	Seed();
	LogIn(Provider::kAniList);
	web.Respond(kSaved, kSaved);
	web.Respond("SaveMediaListEntry", kSaved);
	web.Respond(kEntry, kAhead);
	CHECK(Run({"track", "--private", "anilist", "taxi"}) == 0);
	CHECK(err == "Odd Taxi (a:2) is now hidden from others on AniList.\n");
	CHECK(Has(web.sent.back(), R"("variables":{"m":128547,"h":true}})"));
	CHECK(Waiting().empty());
	// And with every change after it.
	CHECK(Run({"set", "taxi", "score=9"}) == 0);
	CHECK(Has(web.sent.back(), R"("variables":{"m":128547,"c":90,"h":true}})"));
	CHECK(Run({"track", "--public", "anilist", "taxi"}) == 0);
	CHECK(Has(web.sent.back(), R"("h":false)"));

	CHECK(Run({"track", "--private", "mal", "taxi"}) == 2);
	CHECK(Has(err, "MyAnimeList has no private entries"));
	CHECK(Run({"track", "--private", "kitsu", "taxi"}) == 1);
	CHECK(Has(err, "a:2 is not linked to Kitsu; link it first"));
	CHECK(Run({"track", "--private", "anilist", "--pull", "anilist", "taxi"}) == 2);
}

TEST_CASE_FIXTURE(CliFixture, "cli track --pull takes the site's entry as the title's") {
	Seed();
	LogIn(Provider::kAniList);
	web.Respond(kEntry, kAhead);
	CHECK(Run({"track", "--pull", "anilist", "taxi"}) == 0);
	CHECK(out ==
	      "a:2\tOdd Taxi\tprogress\t5\t9\n"
	      "a:2\tOdd Taxi\tscore\t8\t9\n");
	auto taxi = SavedTitle(MediaType::kAnime, "Odd Taxi");
	CHECK(taxi.progress == 9);
	// 85 of 100 is 8.5, kept here as 9 of 10.
	CHECK(taxi.score == 9);
	// AniList Has it already: nothing is sent back.
	CHECK_FALSE(Has(web.sent.back(), "SaveMediaListEntry"));
	CHECK(Waiting().empty());
	CHECK(Run({"undo"}) == 0);
	CHECK(SavedTitle(MediaType::kAnime, "Odd Taxi").progress == 5);
	CHECK(Run({"track", "--pull", "kitsu", "taxi"}) == 1);
}
