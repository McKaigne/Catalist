#include <doctest/doctest.h>

#include "support/cli_fixture.hpp"

using catalist::MediaType;
using catalist::Provider;
using catalist::test::CliFixture;
using catalist::test::Has;

namespace {

const char* kSaved = R"({"data":{"SaveMediaListEntry":{"id":9}}})";
// Your entry on AniList, read before only some fields are sent.
const char* kEntry = "mediaList(userId:$u,mediaId:$m)";
const char* kOnList =
    R"({"data":{"Page":{"mediaList":[{"status":"CURRENT","score":80,"progress":5}]}}})";

}  // namespace

TEST_CASE_FIXTURE(CliFixture, "cli tells a logged-in site of each change, one way") {
	Seed();
	// Not logged in: nothing is sent.
	CHECK(Run({"+", "taxi"}) == 0);
	CHECK(web.sent.empty());

	LogIn(Provider::kAniList);
	web.Respond("SaveMediaListEntry", kSaved);
	web.Respond(kEntry, kOnList);
	// The entry on AniList is read first, and only the progress is sent.
	CHECK(Run({"+", "taxi"}) == 0);
	REQUIRE(web.sent.size() == 2);
	CHECK(Has(web.sent[0], kEntry));
	CHECK(Has(web.sent[1], R"("variables":{"m":128547,"p":7}})"));
	CHECK(err.empty());
	CHECK(Waiting().empty());
	// What the sites do not keep (a title) is not theirs to hear of.
	CHECK(Run({"set", "taxi", "title=ODDTAXI"}) == 0);
	CHECK(web.sent.size() == 2);
	// Undo is a change like any; lowering the progress is meant.
	CHECK(Run({"undo", "-n", "2"}) == 0);
	CHECK(web.sent.size() == 4);
	CHECK(Has(web.sent[3], R"("variables":{"m":128547,"p":6}})"));
}

TEST_CASE_FIXTURE(CliFixture, "cli keeps what it could not send, and sync sends it") {
	Seed();
	LogIn(Provider::kAniList);
	// AniList cannot be reached.
	CHECK(Run({"done", "taxi"}) == 0);
	CHECK(Has(err, "note: could not tell AniList of a:2: could not reach AniList.\n"));
	CHECK(Has(err, "note: 1 change waits to be sent to the sites; 'catalist sync' sends it.\n"));
	REQUIRE(Waiting().size() == 1);
	CHECK(Run({"sync"}) == 1);
	CHECK(Has(err, "error: 1 change still to be sent; try again later\n"));

	web.Respond("SaveMediaListEntry", kSaved);
	web.Respond(kEntry, kOnList);
	CHECK(Run({"sync"}) == 0);
	CHECK(err == "Told the sites of 1 change.\n");
	CHECK(Waiting().empty());
	CHECK(Run({"sync"}) == 0);
	CHECK(err == "Nothing to send.\n");
}

TEST_CASE_FIXTURE(CliFixture, "cli --offline keeps the changes for sync") {
	Seed();
	LogIn(Provider::kAniList);
	web.Respond("SaveMediaListEntry", kSaved);
	CHECK(Run({"--offline", "+", "taxi"}) == 0);
	CHECK(web.sent.empty());
	CHECK(err.empty());
	CHECK(Waiting().size() == 1);
	CHECK(Run({"--offline", "sync"}) == 1);
	web.Respond(kEntry, kOnList);
	CHECK(Run({"sync", "-q"}) == 0);
	CHECK(web.sent.size() == 2);
	CHECK(Waiting().empty());
}

TEST_CASE_FIXTURE(CliFixture, "cli tells a site only of the titles linked to it") {
	Seed();
	LogIn(Provider::kKitsu);
	// No title of the lists Has a Kitsu id yet.
	CHECK(Run({"+", "taxi"}) == 0);
	CHECK(web.sent.empty());
	CHECK(Waiting().empty());
	CHECK(Run({"ls", "-q", "--unlinked", "kitsu"}) == 0);
	CHECK(out == "m:1\na:2\na:1\nm:2\n");

	web.Respond("filter[animeId]=43095", R"({"data":[]})");
	web.Respond("POST https://kitsu.app/api/edge/library-entries", "{}");
	CHECK(Run({"link", "taxi", "kitsu", "43095"}) == 0);
	CHECK(err == "Linked Odd Taxi (a:2) to Kitsu 43095; it is new on your list there.\n");
	// Linking reads your entry there, then sends the title there whole.
	CHECK(web.sent.size() == 3);
	CHECK(Has(web.sent[2], R"("status":"current","reconsuming":false,"progress":6,)"));
	CHECK(SavedTitle(MediaType::kAnime, "Odd Taxi").kitsu_id == 43095);
	CHECK(Run({"ls", "-q", "--unlinked", "kitsu"}) == 0);
	CHECK(out == "m:1\na:1\nm:2\n");
	CHECK(Run({"ls", "-q", "--linked", "kitsu"}) == 0);
	CHECK(out == "a:2\n");
	// On AniList and on Kitsu; on AniList but not on Kitsu.
	CHECK(Run({"ls", "-q", "--linked", "anilist,kitsu"}) == 0);
	CHECK(out == "a:2\n");
	CHECK(Run({"ls", "-q", "--linked", "anilist", "--unlinked", "kitsu"}) == 0);
	CHECK(out == "m:1\na:1\nm:2\n");
	CHECK(Run({"ls", "--linked", "myspace"}) == 2);
	// An id is no step for undo: the one step is the + above.
	CHECK(Steps().size() == 1);

	CHECK(Run({"link", "frieren", "kitsu", "43095"}, "", 0) == 1);
	CHECK(Run({"link", "-a", "frieren", "kitsu", "43095"}) == 1);
	CHECK(Has(err, "error: a:2 is Kitsu 43095 already\n"));
	CHECK(Run({"unlink", "taxi", "kitsu"}) == 0);
	CHECK(err == "Unlinked Odd Taxi (a:2) from Kitsu: it stays on your list there.\n");
	CHECK(SavedTitle(MediaType::kAnime, "Odd Taxi").kitsu_id == 0);
	CHECK(Run({"unlink", "taxi", "kitsu"}) == 0);
	CHECK(err == "a:2 is not linked to Kitsu.\n");
	CHECK(Run({"link", "taxi", "myspace"}) == 2);
}

TEST_CASE_FIXTURE(CliFixture, "cli link asks the site before it takes the lists for writing") {
	Seed();
	REQUIRE(Run({"add", "--no-lookup", "Dandadan"}) == 0);
	web.Respond(R"("search":"Dandadan","type":"ANIME")", R"({"data":{"Page":{"media":[
	  {"id":171018,"idMal":57334,"title":{"romaji":"Dandadan"},"episodes":12}]}}})");
	// Another program can write while the site is asked.
	bool others_could_write = false;
	web.while_asked = [&others_could_write] {
		catalist::Catalogue other("data");
		others_could_write = other.BeginWrite().has_value();
	};
	CHECK(Run({"link", "dandadan", "anilist"}) == 0);
	CHECK(others_could_write);
	CHECK(err == "Linked Dandadan (a:3) to AniList 171018.\n");
	auto linked = SavedTitle(MediaType::kAnime, "Dandadan");
	CHECK(linked.anilist_id == 171018);
	CHECK(linked.mal_id == 57334);
}

TEST_CASE_FIXTURE(CliFixture, "cli link folds your entry on the site in, as one step") {
	Seed();
	LogIn(Provider::kAniList);
	CHECK(Run({"unlink", "taxi", "anilist"}) == 0);
	web.Respond("SaveMediaListEntry", kSaved);
	// On AniList you are further along, and scored it 85 of 100.
	web.Respond(kEntry, R"({"data":{"Page":{"mediaList":[{"status":"CURRENT","score":85,
	  "progress":9,"startedAt":{"year":2026,"month":9,"day":30}}]}}})");
	CHECK(Run({"link", "taxi", "anilist", "128547"}) == 0);
	CHECK(out == "a:2\tOdd Taxi\tprogress\t5\t9\n");
	CHECK(err == "Linked Odd Taxi (a:2) to AniList 128547; your entry there is folded in.\n");
	auto taxi = SavedTitle(MediaType::kAnime, "Odd Taxi");
	CHECK(taxi.progress == 9);
	// Your own score and start date stay.
	CHECK(taxi.score == 8);
	CHECK(catalist::FormatDate(taxi.start_date) == "2026-10-01");
	// AniList is then sent the title whole.
	CHECK(Has(web.sent.back(), R"("variables":{"m":128547,"s":"CURRENT","c":80,"p":9,)"));
	CHECK(Waiting().empty());
	// The fold is a step for undo.
	CHECK(Run({"undo"}) == 0);
	CHECK(SavedTitle(MediaType::kAnime, "Odd Taxi").progress == 5);

	// Searched for under another name.
	CHECK(Run({"unlink", "taxi", "anilist"}) == 0);
	web.Respond(R"("search":"odd taxi tv","type":"ANIME")", R"({"data":{"Page":{"media":[
	  {"id":128547,"idMal":44265,"title":{"romaji":"Odd Taxi"}}]}}})");
	CHECK(Run({"link", "--query", "odd taxi tv", "taxi", "anilist"}) == 0);
	CHECK(SavedTitle(MediaType::kAnime, "Odd Taxi").anilist_id == 128547);
	CHECK(Run({"link", "--query", "x", "taxi", "anilist", "1"}) == 2);
	// AniList not reached: nothing is linked.
	CHECK(Run({"unlink", "taxi", "anilist"}) == 0);
	web.bodies.erase(kEntry);
	CHECK(Run({"link", "taxi", "anilist", "128547"}) == 1);
	CHECK(Has(err, "Nothing was linked."));
	CHECK(SavedTitle(MediaType::kAnime, "Odd Taxi").anilist_id == 0);
}

TEST_CASE_FIXTURE(CliFixture, "cli rm leaves the sites' lists alone, unless --remote") {
	Seed();
	LogIn(Provider::kAniList);
	CHECK(Run({"rm", "-f", "taxi"}) == 0);
	CHECK(web.sent.empty());
	web.Respond("SaveMediaListEntry", kSaved);
	CHECK(Run({"undo"}) == 0);
	web.Respond("MediaList(mediaId:$m,userId:$u)", R"({"data":{"MediaList":{"id":31}}})");
	web.Respond("DeleteMediaListEntry", R"({"data":{"DeleteMediaListEntry":{"deleted":true}}})");
	size_t before = web.sent.size();
	CHECK(Run({"rm", "-f", "--remote", "taxi"}) == 0);
	REQUIRE(web.sent.size() == before + 2);
	CHECK(Has(web.sent.back(), R"("i":31)"));
	CHECK(Waiting().empty());
}

TEST_CASE_FIXTURE(CliFixture, "cli unlink --remote takes the title off the site's list") {
	Seed();
	CHECK(Run({"unlink", "--remote", "taxi", "anilist"}) == 1);
	CHECK(Has(err, "you are not logged in to AniList"));
	// Unlinked all the same.
	CHECK(SavedTitle(MediaType::kAnime, "Odd Taxi").anilist_id == 0);
	CHECK(Run({"link", "taxi", "anilist", "128547"}) == 0);

	LogIn(Provider::kAniList);
	web.Respond("MediaList(mediaId:$m,userId:$u)", R"({"data":{"MediaList":{"id":31}}})");
	web.Respond("DeleteMediaListEntry", R"({"data":{"DeleteMediaListEntry":{"deleted":true}}})");
	CHECK(Run({"unlink", "--remote", "taxi", "anilist"}) == 0);
	CHECK(err == "Unlinked Odd Taxi (a:2) from AniList; it comes off your list there.\n");
	CHECK(Has(web.sent.back(), R"("i":31)"));
	CHECK(Waiting().empty());
}

TEST_CASE_FIXTURE(CliFixture, "cli a title a site refuses does not hold back the others") {
	Seed();
	LogIn(Provider::kAniList);
	web.Respond("SaveMediaListEntry", kSaved);
	web.Respond(R"("variables":{"m":154587)", R"({"errors":[{"message":"validation"}]})", 400);
	CHECK(Run({"sync", "--all"}) == 1);
	CHECK(Has(err, "note: could not tell AniList of a:1: AniList said no (HTTP 400).\n"));
	CHECK(Has(err, "Told the sites of 3 changes.\n"));
	REQUIRE(Waiting().size() == 1);
	CHECK(Waiting()[0].id == 1);
}

TEST_CASE_FIXTURE(CliFixture, "cli lets a site further along keep its progress") {
	Seed();
	LogIn(Provider::kAniList);
	web.Respond("SaveMediaListEntry", kSaved);
	web.Respond(kEntry, R"({"data":{"Page":{"mediaList":[{"status":"CURRENT","progress":10}]}}})");
	// Odd Taxi goes from 5 to 6 here; AniList Has 10: nothing to send.
	CHECK(Run({"+", "taxi"}) == 0);
	REQUIRE(web.sent.size() == 1);
	CHECK(Has(web.sent[0], kEntry));
	CHECK(Waiting().empty());
	// The score is sent, the progress still left as AniList Has it.
	CHECK(Run({"+", "taxi"}) == 0);
	CHECK(Run({"set", "taxi", "score=9"}) == 0);
	CHECK(Has(web.sent.back(), R"("variables":{"m":128547,"c":90}})"));
	// Lowering it on purpose is sent.
	CHECK(Run({"-", "taxi"}) == 0);
	CHECK(Has(web.sent.back(), R"("variables":{"m":128547,"p":6}})"));
}

TEST_CASE_FIXTURE(CliFixture, "cli sends a site without the title all of it") {
	Seed();
	LogIn(Provider::kAniList);
	web.Respond("SaveMediaListEntry", kSaved);
	web.Respond(kEntry, R"({"data":{"Page":{"mediaList":[]}}})");
	CHECK(Run({"+", "taxi"}) == 0);
	REQUIRE(web.sent.size() == 2);
	CHECK(Has(web.sent[1], R"("variables":{"m":128547,"s":"CURRENT","c":80,"p":6,)"));
}

TEST_CASE_FIXTURE(CliFixture, "cli waits longer after each failure, and gives a refusal up") {
	Seed();
	LogIn(Provider::kAniList);
	// AniList cannot be reached: the change waits five minutes.
	CHECK(Run({"done", "taxi"}) == 0);
	REQUIRE(Waiting().size() == 1);
	CHECK(Waiting()[0].attempts == 1);
	CHECK(Waiting()[0].next_try == clock + 300);
	// Another command that goes online sends only what is due.
	size_t before = web.sent.size();
	CHECK(Run({"search", "taxi"}) == 1);
	CHECK(web.sent.size() == before + 1);
	clock += 301;
	CHECK(Run({"search", "taxi"}) == 1);
	CHECK(Waiting()[0].attempts == 2);
	CHECK(Waiting()[0].next_try == clock + 600);
	// What waits, and when it is tried again.
	CHECK(Run({"sync", "--list"}) == 0);
	CHECK(out == "a:2\tanilist\tstatus, progress (up only), finished\t2\t600\n");
	CHECK(Run({"sync", "--list"}, "", 100) == 0);
	CHECK(Has(out, "a:2  AniList  status, progress (up only), finished"));
	CHECK(Has(out, "  2  in 10 min\n"));
	// AniList says no to it, three times in all: it is given up.
	web.Respond(kEntry, kOnList);
	web.Respond("SaveMediaListEntry", R"({"errors":[{"message":"validation"}]})", 400);
	CHECK(Run({"sync"}) == 1);
	CHECK(Has(err,
	          "note: gave up telling AniList of a:2 after 3 tries: AniList said no (HTTP 400).\n"));
	CHECK(Waiting().empty());
	CHECK(Run({"sync", "--list"}, "", 100) == 0);
	CHECK(err == "Nothing waits to be sent.\n");
}

TEST_CASE_FIXTURE(CliFixture, "cli sync --all sends every linked title") {
	Seed();
	CHECK(Run({"sync"}) == 0);
	CHECK(err == "You are not logged in to any site. Log in with: catalist login SITE\n");
	LogIn(Provider::kAniList);
	web.Respond("SaveMediaListEntry", kSaved);
	CHECK(Run({"sync", "--all"}) == 0);
	// The four titles of the lists all have an AniList id.
	CHECK(web.sent.size() == 4);
	CHECK(Has(err, "Told the sites of 4 changes.\n"));
	CHECK(Run({"sync", "--forget"}) == 0);
	CHECK(err == "Forgot 0 changes the sites were not told of.\n");
}
