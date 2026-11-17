#include <doctest/doctest.h>

#include "support/cliFixture.hpp"

using catalist::mediaType;
using catalist::provider;
using catalist::test::cliFixture;
using catalist::test::has;

namespace {

const char* kSaved = R"({"data":{"SaveMediaListEntry":{"id":9}}})";
// Your entry on AniList, read before only some fields are sent.
const char* kEntry = "mediaList(userId:$u,mediaId:$m)";
const char* kOnList =
    R"({"data":{"Page":{"mediaList":[{"status":"CURRENT","score":80,"progress":5}]}}})";

}  // namespace

TEST_CASE_FIXTURE(cliFixture, "cli tells a logged-in site of each change, one way") {
	seed();
	// Not logged in: nothing is sent.
	CHECK(run({"+", "taxi"}) == 0);
	CHECK(web.sent.empty());

	logIn(provider::kAniList);
	web.respond("SaveMediaListEntry", kSaved);
	web.respond(kEntry, kOnList);
	// The entry on AniList is read first, and only the progress is sent.
	CHECK(run({"+", "taxi"}) == 0);
	REQUIRE(web.sent.size() == 2);
	CHECK(has(web.sent[0], kEntry));
	CHECK(has(web.sent[1], R"("variables":{"m":128547,"p":7}})"));
	CHECK(err.empty());
	CHECK(waiting().empty());
	// What the sites do not keep (a title) is not theirs to hear of.
	CHECK(run({"set", "taxi", "title=ODDTAXI"}) == 0);
	CHECK(web.sent.size() == 2);
	// Undo is a change like any; lowering the progress is meant.
	CHECK(run({"undo", "-n", "2"}) == 0);
	CHECK(web.sent.size() == 4);
	CHECK(has(web.sent[3], R"("variables":{"m":128547,"p":6}})"));
}

TEST_CASE_FIXTURE(cliFixture, "cli keeps what it could not send, and sync sends it") {
	seed();
	logIn(provider::kAniList);
	// AniList cannot be reached.
	CHECK(run({"done", "taxi"}) == 0);
	CHECK(has(err,
	          "note: could not tell AniList of a:2: could not reach AniList.\n"));
	CHECK(has(err, "note: 1 change waits to be sent to the sites; 'catalist sync' sends it.\n"));
	REQUIRE(waiting().size() == 1);
	CHECK(run({"sync"}) == 1);
	CHECK(has(err, "error: 1 change still to be sent; try again later\n"));

	web.respond("SaveMediaListEntry", kSaved);
	web.respond(kEntry, kOnList);
	CHECK(run({"sync"}) == 0);
	CHECK(err == "Told the sites of 1 change.\n");
	CHECK(waiting().empty());
	CHECK(run({"sync"}) == 0);
	CHECK(err == "Nothing to send.\n");
}

TEST_CASE_FIXTURE(cliFixture, "cli --offline keeps the changes for sync") {
	seed();
	logIn(provider::kAniList);
	web.respond("SaveMediaListEntry", kSaved);
	CHECK(run({"--offline", "+", "taxi"}) == 0);
	CHECK(web.sent.empty());
	CHECK(err.empty());
	CHECK(waiting().size() == 1);
	CHECK(run({"--offline", "sync"}) == 1);
	web.respond(kEntry, kOnList);
	CHECK(run({"sync", "-q"}) == 0);
	CHECK(web.sent.size() == 2);
	CHECK(waiting().empty());
}

TEST_CASE_FIXTURE(cliFixture, "cli tells a site only of the titles linked to it") {
	seed();
	logIn(provider::kKitsu);
	// No title of the lists has a Kitsu id yet.
	CHECK(run({"+", "taxi"}) == 0);
	CHECK(web.sent.empty());
	CHECK(waiting().empty());
	CHECK(run({"ls", "-q", "--unlinked", "kitsu"}) == 0);
	CHECK(out == "m:1\na:2\na:1\nm:2\n");

	web.respond("filter[animeId]=43095", R"({"data":[]})");
	web.respond("POST https://kitsu.app/api/edge/library-entries", "{}");
	CHECK(run({"link", "taxi", "kitsu", "43095"}) == 0);
	CHECK(err == "Linked Odd Taxi (a:2) to Kitsu 43095; it is new on your list there.\n");
	// Linking reads your entry there, then sends the title there whole.
	CHECK(web.sent.size() == 3);
	CHECK(has(web.sent[2], R"("status":"current","reconsuming":false,"progress":6,)"));
	CHECK(savedTitle(mediaType::kAnime, "Odd Taxi").kitsuId == 43095);
	CHECK(run({"ls", "-q", "--unlinked", "kitsu"}) == 0);
	CHECK(out == "m:1\na:1\nm:2\n");
	CHECK(run({"ls", "-q", "--linked", "kitsu"}) == 0);
	CHECK(out == "a:2\n");
	// On AniList and on Kitsu; on AniList but not on Kitsu.
	CHECK(run({"ls", "-q", "--linked", "anilist,kitsu"}) == 0);
	CHECK(out == "a:2\n");
	CHECK(run({"ls", "-q", "--linked", "anilist", "--unlinked", "kitsu"}) == 0);
	CHECK(out == "m:1\na:1\nm:2\n");
	CHECK(run({"ls", "--linked", "myspace"}) == 2);
	// An id is no step for undo: the one step is the + above.
	CHECK(steps().size() == 1);

	CHECK(run({"link", "frieren", "kitsu", "43095"}, "", 0) == 1);
	CHECK(run({"link", "-a", "frieren", "kitsu", "43095"}) == 1);
	CHECK(has(err, "error: a:2 is Kitsu 43095 already\n"));
	CHECK(run({"unlink", "taxi", "kitsu"}) == 0);
	CHECK(err == "Unlinked Odd Taxi (a:2) from Kitsu: it stays on your list there.\n");
	CHECK(savedTitle(mediaType::kAnime, "Odd Taxi").kitsuId == 0);
	CHECK(run({"unlink", "taxi", "kitsu"}) == 0);
	CHECK(err == "a:2 is not linked to Kitsu.\n");
	CHECK(run({"link", "taxi", "myspace"}) == 2);
}

TEST_CASE_FIXTURE(cliFixture, "cli link asks the site before it takes the lists for writing") {
	seed();
	REQUIRE(run({"add", "--no-lookup", "Dandadan"}) == 0);
	web.respond(R"("search":"Dandadan","type":"ANIME")", R"({"data":{"Page":{"media":[
	  {"id":171018,"idMal":57334,"title":{"romaji":"Dandadan"},"episodes":12}]}}})");
	// Another program can write while the site is asked.
	bool othersCouldWrite = false;
	web.whileAsked = [&othersCouldWrite] {
		catalist::catalogue other("data");
		othersCouldWrite = other.beginWrite().has_value();
	};
	CHECK(run({"link", "dandadan", "anilist"}) == 0);
	CHECK(othersCouldWrite);
	CHECK(err == "Linked Dandadan (a:3) to AniList 171018.\n");
	auto linked = savedTitle(mediaType::kAnime, "Dandadan");
	CHECK(linked.anilistId == 171018);
	CHECK(linked.malId == 57334);
}

TEST_CASE_FIXTURE(cliFixture, "cli link folds your entry on the site in, as one step") {
	seed();
	logIn(provider::kAniList);
	CHECK(run({"unlink", "taxi", "anilist"}) == 0);
	web.respond("SaveMediaListEntry", kSaved);
	// On AniList you are further along, and scored it 85 of 100.
	web.respond(kEntry, R"({"data":{"Page":{"mediaList":[{"status":"CURRENT","score":85,
	  "progress":9,"startedAt":{"year":2026,"month":9,"day":30}}]}}})");
	CHECK(run({"link", "taxi", "anilist", "128547"}) == 0);
	CHECK(out == "a:2\tOdd Taxi\tprogress\t5\t9\n");
	CHECK(err == "Linked Odd Taxi (a:2) to AniList 128547; your entry there is folded in.\n");
	auto taxi = savedTitle(mediaType::kAnime, "Odd Taxi");
	CHECK(taxi.progress == 9);
	// Your own score and start date stay.
	CHECK(taxi.score == 8);
	CHECK(catalist::formatDate(taxi.startDate) == "2026-10-01");
	// AniList is then sent the title whole.
	CHECK(has(web.sent.back(), R"("variables":{"m":128547,"s":"CURRENT","c":80,"p":9,)"));
	CHECK(waiting().empty());
	// The fold is a step for undo.
	CHECK(run({"undo"}) == 0);
	CHECK(savedTitle(mediaType::kAnime, "Odd Taxi").progress == 5);

	// Searched for under another name.
	CHECK(run({"unlink", "taxi", "anilist"}) == 0);
	web.respond(R"("search":"odd taxi tv","type":"ANIME")", R"({"data":{"Page":{"media":[
	  {"id":128547,"idMal":44265,"title":{"romaji":"Odd Taxi"}}]}}})");
	CHECK(run({"link", "--query", "odd taxi tv", "taxi", "anilist"}) == 0);
	CHECK(savedTitle(mediaType::kAnime, "Odd Taxi").anilistId == 128547);
	CHECK(run({"link", "--query", "x", "taxi", "anilist", "1"}) == 2);
	// AniList not reached: nothing is linked.
	CHECK(run({"unlink", "taxi", "anilist"}) == 0);
	web.bodies.erase(kEntry);
	CHECK(run({"link", "taxi", "anilist", "128547"}) == 1);
	CHECK(has(err, "Nothing was linked."));
	CHECK(savedTitle(mediaType::kAnime, "Odd Taxi").anilistId == 0);
}

TEST_CASE_FIXTURE(cliFixture, "cli rm leaves the sites' lists alone, unless --remote") {
	seed();
	logIn(provider::kAniList);
	CHECK(run({"rm", "-f", "taxi"}) == 0);
	CHECK(web.sent.empty());
	web.respond("SaveMediaListEntry", kSaved);
	CHECK(run({"undo"}) == 0);
	web.respond("MediaList(mediaId:$m,userId:$u)", R"({"data":{"MediaList":{"id":31}}})");
	web.respond("DeleteMediaListEntry", R"({"data":{"DeleteMediaListEntry":{"deleted":true}}})");
	size_t before = web.sent.size();
	CHECK(run({"rm", "-f", "--remote", "taxi"}) == 0);
	REQUIRE(web.sent.size() == before + 2);
	CHECK(has(web.sent.back(), R"("i":31)"));
	CHECK(waiting().empty());
}

TEST_CASE_FIXTURE(cliFixture, "cli unlink --remote takes the title off the site's list") {
	seed();
	CHECK(run({"unlink", "--remote", "taxi", "anilist"}) == 1);
	CHECK(has(err, "you are not logged in to AniList"));
	// Unlinked all the same.
	CHECK(savedTitle(mediaType::kAnime, "Odd Taxi").anilistId == 0);
	CHECK(run({"link", "taxi", "anilist", "128547"}) == 0);

	logIn(provider::kAniList);
	web.respond("MediaList(mediaId:$m,userId:$u)", R"({"data":{"MediaList":{"id":31}}})");
	web.respond("DeleteMediaListEntry", R"({"data":{"DeleteMediaListEntry":{"deleted":true}}})");
	CHECK(run({"unlink", "--remote", "taxi", "anilist"}) == 0);
	CHECK(err == "Unlinked Odd Taxi (a:2) from AniList; it comes off your list there.\n");
	CHECK(has(web.sent.back(), R"("i":31)"));
	CHECK(waiting().empty());
}

TEST_CASE_FIXTURE(cliFixture, "cli a title a site refuses does not hold back the others") {
	seed();
	logIn(provider::kAniList);
	web.respond("SaveMediaListEntry", kSaved);
	web.respond(R"("variables":{"m":154587)", R"({"errors":[{"message":"validation"}]})", 400);
	CHECK(run({"sync", "--all"}) == 1);
	CHECK(has(err, "note: could not tell AniList of a:1: AniList said no (HTTP 400).\n"));
	CHECK(has(err, "Told the sites of 3 changes.\n"));
	REQUIRE(waiting().size() == 1);
	CHECK(waiting()[0].id == 1);
}

TEST_CASE_FIXTURE(cliFixture, "cli lets a site further along keep its progress") {
	seed();
	logIn(provider::kAniList);
	web.respond("SaveMediaListEntry", kSaved);
	web.respond(kEntry,
	            R"({"data":{"Page":{"mediaList":[{"status":"CURRENT","progress":10}]}}})");
	// Odd Taxi goes from 5 to 6 here; AniList has 10: nothing to send.
	CHECK(run({"+", "taxi"}) == 0);
	REQUIRE(web.sent.size() == 1);
	CHECK(has(web.sent[0], kEntry));
	CHECK(waiting().empty());
	// The score is sent, the progress still left as AniList has it.
	CHECK(run({"+", "taxi"}) == 0);
	CHECK(run({"set", "taxi", "score=9"}) == 0);
	CHECK(has(web.sent.back(), R"("variables":{"m":128547,"c":90}})"));
	// Lowering it on purpose is sent.
	CHECK(run({"-", "taxi"}) == 0);
	CHECK(has(web.sent.back(), R"("variables":{"m":128547,"p":6}})"));
}

TEST_CASE_FIXTURE(cliFixture, "cli sends a site without the title all of it") {
	seed();
	logIn(provider::kAniList);
	web.respond("SaveMediaListEntry", kSaved);
	web.respond(kEntry, R"({"data":{"Page":{"mediaList":[]}}})");
	CHECK(run({"+", "taxi"}) == 0);
	REQUIRE(web.sent.size() == 2);
	CHECK(has(web.sent[1], R"("variables":{"m":128547,"s":"CURRENT","c":80,"p":6,)"));
}

TEST_CASE_FIXTURE(cliFixture, "cli waits longer after each failure, and gives a refusal up") {
	seed();
	logIn(provider::kAniList);
	// AniList cannot be reached: the change waits five minutes.
	CHECK(run({"done", "taxi"}) == 0);
	REQUIRE(waiting().size() == 1);
	CHECK(waiting()[0].attempts == 1);
	CHECK(waiting()[0].nextTry == clock + 300);
	// Another command that goes online sends only what is due.
	size_t before = web.sent.size();
	CHECK(run({"search", "taxi"}) == 1);
	CHECK(web.sent.size() == before + 1);
	clock += 301;
	CHECK(run({"search", "taxi"}) == 1);
	CHECK(waiting()[0].attempts == 2);
	CHECK(waiting()[0].nextTry == clock + 600);
	// What waits, and when it is tried again.
	CHECK(run({"sync", "--list"}) == 0);
	CHECK(out == "a:2\tanilist\tstatus, progress (up only), finished\t2\t600\n");
	CHECK(run({"sync", "--list"}, "", 100) == 0);
	CHECK(has(out, "a:2  AniList  status, progress (up only), finished"));
	CHECK(has(out, "  2  in 10 min\n"));
	// AniList says no to it, three times in all: it is given up.
	web.respond(kEntry, kOnList);
	web.respond("SaveMediaListEntry", R"({"errors":[{"message":"validation"}]})", 400);
	CHECK(run({"sync"}) == 1);
	CHECK(has(err, "note: gave up telling AniList of a:2 after 3 tries: AniList said no (HTTP 400).\n"));
	CHECK(waiting().empty());
	CHECK(run({"sync", "--list"}, "", 100) == 0);
	CHECK(err == "Nothing waits to be sent.\n");
}

TEST_CASE_FIXTURE(cliFixture, "cli sync --all sends every linked title") {
	seed();
	CHECK(run({"sync"}) == 0);
	CHECK(err == "You are not logged in to any site. Log in with: catalist login SITE\n");
	logIn(provider::kAniList);
	web.respond("SaveMediaListEntry", kSaved);
	CHECK(run({"sync", "--all"}) == 0);
	// The four titles of the lists all have an AniList id.
	CHECK(web.sent.size() == 4);
	CHECK(has(err, "Told the sites of 4 changes.\n"));
	CHECK(run({"sync", "--forget"}) == 0);
	CHECK(err == "Forgot 0 changes the sites were not told of.\n");
}
