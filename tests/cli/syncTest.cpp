#include <doctest/doctest.h>

#include "support/cliFixture.hpp"

using catalist::mediaType;
using catalist::provider;
using catalist::test::cliFixture;
using catalist::test::has;

namespace {

const char* kSaved = R"({"data":{"SaveMediaListEntry":{"id":9}}})";

}  // namespace

TEST_CASE_FIXTURE(cliFixture, "cli tells a logged-in site of each change, one way") {
	seed();
	// Not logged in: nothing is sent.
	CHECK(run({"+", "taxi"}) == 0);
	CHECK(web.sent.empty());

	logIn(provider::kAniList);
	web.respond("SaveMediaListEntry", kSaved);
	CHECK(run({"+", "taxi"}) == 0);
	REQUIRE(web.sent.size() == 1);
	CHECK(has(web.sent[0], R"("m":128547,"s":"CURRENT","c":80,"p":7,)"));
	CHECK(err.empty());
	CHECK(waiting().empty());
	// What the sites do not keep (a title) is not theirs to hear of.
	CHECK(run({"set", "taxi", "title=ODDTAXI"}) == 0);
	CHECK(web.sent.size() == 1);
	// Undo is a change like any.
	CHECK(run({"undo", "-n", "2"}) == 0);
	CHECK(web.sent.size() == 2);
	CHECK(has(web.sent[1], R"("p":6,)"));
}

TEST_CASE_FIXTURE(cliFixture, "cli keeps what it could not send, and sync sends it") {
	seed();
	logIn(provider::kAniList);
	// AniList cannot be reached.
	CHECK(run({"done", "taxi"}) == 0);
	CHECK(has(err,
	          "note: could not tell AniList of a:2: could not reach AniList, or it did not "
	          "take the change.\n"));
	CHECK(has(err, "note: 1 change waits to be sent to the sites; 'catalist sync' sends it.\n"));
	REQUIRE(waiting().size() == 1);
	CHECK(run({"sync"}) == 1);
	CHECK(has(err, "error: 1 change still to be sent; try again later\n"));

	web.respond("SaveMediaListEntry", kSaved);
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
	CHECK(run({"sync", "-q"}) == 0);
	CHECK(web.sent.size() == 1);
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
	CHECK(err == "Linked Odd Taxi (a:2) to Kitsu 43095.\n");
	// Linking sends the title there.
	CHECK(web.sent.size() == 2);
	CHECK(savedTitle(mediaType::kAnime, "Odd Taxi").kitsuId == 43095);
	CHECK(run({"ls", "-q", "--unlinked", "kitsu"}) == 0);
	CHECK(out == "m:1\na:1\nm:2\n");
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

TEST_CASE_FIXTURE(cliFixture, "cli rm leaves the sites' lists alone, unless --remote") {
	seed();
	logIn(provider::kAniList);
	CHECK(run({"rm", "-f", "taxi"}) == 0);
	CHECK(web.sent.empty());
	CHECK(run({"undo"}) == 0);
	web.respond("SaveMediaListEntry", kSaved);
	web.respond("MediaList(mediaId:$m,userId:$u)", R"({"data":{"MediaList":{"id":31}}})");
	web.respond("DeleteMediaListEntry", R"({"data":{"DeleteMediaListEntry":{"deleted":true}}})");
	size_t before = web.sent.size();
	CHECK(run({"rm", "-f", "--remote", "taxi"}) == 0);
	REQUIRE(web.sent.size() == before + 2);
	CHECK(has(web.sent.back(), R"("i":31)"));
	CHECK(waiting().empty());
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
