#include <doctest/doctest.h>

#include "support/cliFixture.hpp"

using catalist::mediaType;
using catalist::provider;
using catalist::watchStatus;
using catalist::test::cliFixture;
using catalist::test::has;

namespace {

const char* kSaved = R"({"data":{"SaveMediaListEntry":{"id":9}}})";
const char* kEntry = "mediaList(userId:$u,mediaId:$m)";
// Odd Taxi on AniList: further along, scored 85 of 100, hidden.
const char* kAhead = R"({"data":{"Page":{"mediaList":[{"status":"CURRENT","score":85,
  "progress":9,"repeat":0,"private":true,"startedAt":{"year":2026,"month":10,"day":1},
  "completedAt":{}}]}}})";

}  // namespace

TEST_CASE_FIXTURE(cliFixture, "cli track shows a title on each site beside the lists") {
	seed();
	logIn(provider::kAniList);
	web.respond(kEntry, kAhead);
	CHECK(run({"track", "taxi"}) == 0);
	CHECK(out ==
	      "anilist\t128547\tlisted\twatching\t85\t9\t0\t0\t2026-10-01\t-\ttrue\tscore,progress\t"
	      "https://anilist.co/anime/128547\n"
	      "mal\t44265\tlogged-out\t-\t-\t-\t-\t-\t-\t-\t-\t-\thttps://myanimelist.net/anime/44265\n"
	      "kitsu\t0\tunlinked\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\n");
	CHECK(run({"track", "--json", "taxi"}) == 0);
	CHECK(has(out, R"({"site":"anilist","id":128547,"state":"listed","status":"watching","score":85,)"));
	CHECK(has(out, R"("hidden":true,"differs":"score,progress",)"));

	// At a terminal, with what to do about it.
	CHECK(run({"track", "taxi"}, "", 120) == 0);
	CHECK(has(out, "Odd Taxi\nSITE "));
	CHECK(has(out, "AniList      128547  Watching"));
	CHECK(has(out, "  9/13      2026-10-01  -         hidden\n"));
	CHECK(has(out, "MyAnimeList  44265   not logged in\n"));
	CHECK(has(out, "Kitsu        -       not linked\n"));
	CHECK(has(err, "'catalist track --pull anilist a:2' takes AniList's as the title's"));

	// Not on your list there, or not reached.
	web.respond(kEntry, R"({"data":{"Page":{"mediaList":[]}}})");
	CHECK(run({"track", "taxi"}) == 0);
	CHECK(has(out, "anilist\t128547\tmissing\t"));
	web.bodies.erase(kEntry);
	CHECK(run({"track", "taxi"}) == 0);
	CHECK(has(out, "anilist\t128547\tfailed\t"));
	CHECK(run({"track", "taxi", "frieren"}) == 2);
}

TEST_CASE_FIXTURE(cliFixture, "cli track --private hides a title on a site") {
	seed();
	logIn(provider::kAniList);
	web.respond(kSaved, kSaved);
	web.respond("SaveMediaListEntry", kSaved);
	web.respond(kEntry, kAhead);
	CHECK(run({"track", "--private", "anilist", "taxi"}) == 0);
	CHECK(err == "Odd Taxi (a:2) is now hidden from others on AniList.\n");
	CHECK(has(web.sent.back(), R"("variables":{"m":128547,"h":true}})"));
	CHECK(waiting().empty());
	// And with every change after it.
	CHECK(run({"set", "taxi", "score=9"}) == 0);
	CHECK(has(web.sent.back(), R"("variables":{"m":128547,"c":90,"h":true}})"));
	CHECK(run({"track", "--public", "anilist", "taxi"}) == 0);
	CHECK(has(web.sent.back(), R"("h":false)"));

	CHECK(run({"track", "--private", "mal", "taxi"}) == 2);
	CHECK(has(err, "MyAnimeList has no private entries"));
	CHECK(run({"track", "--private", "kitsu", "taxi"}) == 1);
	CHECK(has(err, "a:2 is not linked to Kitsu; link it first"));
	CHECK(run({"track", "--private", "anilist", "--pull", "anilist", "taxi"}) == 2);
}

TEST_CASE_FIXTURE(cliFixture, "cli track --pull takes the site's entry as the title's") {
	seed();
	logIn(provider::kAniList);
	web.respond(kEntry, kAhead);
	CHECK(run({"track", "--pull", "anilist", "taxi"}) == 0);
	CHECK(out ==
	      "a:2\tOdd Taxi\tprogress\t5\t9\n"
	      "a:2\tOdd Taxi\tscore\t8\t9\n");
	auto taxi = savedTitle(mediaType::kAnime, "Odd Taxi");
	CHECK(taxi.progress == 9);
	// 85 of 100 is 8.5, kept here as 9 of 10.
	CHECK(taxi.score == 9);
	// AniList has it already: nothing is sent back.
	CHECK_FALSE(has(web.sent.back(), "SaveMediaListEntry"));
	CHECK(waiting().empty());
	CHECK(run({"undo"}) == 0);
	CHECK(savedTitle(mediaType::kAnime, "Odd Taxi").progress == 5);
	CHECK(run({"track", "--pull", "kitsu", "taxi"}) == 1);
}
