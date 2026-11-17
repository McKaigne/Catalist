#include <doctest/doctest.h>

#include <filesystem>

#include "support/cli_fixture.hpp"

using catalist::MediaType;
using catalist::WatchStatus;
using catalist::test::CliFixture;
using catalist::test::Has;

TEST_CASE_FIXTURE(CliFixture, "cli add finds the title on AniList and adds it") {
	Seed();
	web.Respond(R"("search":"dandadan","type":"ANIME")", R"({"data":{"Page":{"media":[
	  {"id":171018,"idMal":57334,"title":{"romaji":"Dandadan"},"episodes":12,
	   "startDate":{"year":2024}}]}}})");
	web.Respond(R"({"id":171018,"type":"ANIME")", R"({"data":{"Media":{"id":171018,
	  "idMal":57334,"title":{"romaji":"Dandadan"},"episodes":12,"status":"FINISHED"}}})");
	CHECK(Run({"add", "--status", "watching", "dandadan"}) == 0);
	CHECK(out == "a:3\tDandadan\tadded\t-\twatching\n");
	CHECK(err.empty());
	auto added = SavedTitle(MediaType::kAnime, "Dandadan");
	CHECK(added.anilist_id == 171018);
	CHECK(added.mal_id == 57334);
	CHECK(added.total_units == 12);
	CHECK(added.released == 12);
	CHECK(added.progress == 0);
	CHECK(catalist::FormatDate(added.start_date) == "2026-10-20");
	CHECK(catalist::FormatDate(added.updated_at) == "2026-10-20");

	// Once is enough.
	CHECK(Run({"add", "dandadan"}) == 0);
	CHECK(err == "Dandadan is in your anime list already, as a:3 (watching).\nNothing changed.\n");
	CHECK(out.empty());
	CHECK(Saved(MediaType::kAnime).size() == 3);

	// One step for undo, and the ref is free again.
	REQUIRE(Steps().size() == 1);
	CHECK(Steps()[0].label == "add --status watching dandadan");
	CHECK(Run({"undo"}) == 0);
	CHECK(Saved(MediaType::kAnime).size() == 2);
	CHECK(Run({"add", "-q", "--status=completed", "dandadan"}) == 0);
	CHECK(out.empty());
	added = SavedTitle(MediaType::kAnime, "Dandadan");
	CHECK(added.id == 3);
	CHECK(added.status == WatchStatus::kCompleted);
	CHECK(added.progress == 12);
}

TEST_CASE_FIXTURE(CliFixture, "cli add -m adds a manga, planned unless told otherwise") {
	web.Respond(R"("search":"vagabond","type":"MANGA")", R"({"data":{"Page":{"media":[
	  {"id":30656,"idMal":656,"title":{"romaji":"Vagabond"},"chapters":327,
	   "startDate":{"year":1998}}]}}})");
	web.Respond(R"({"id":30656,"type":"MANGA")", R"({"data":{"Media":{"id":30656,
	  "idMal":656,"title":{"romaji":"Vagabond"},"chapters":327,"volumes":37,
	  "status":"HIATUS"}}})");
	CHECK(Run({"add", "-m", "vagabond"}, "", 100) == 0);
	CHECK(out == "Vagabond  added as planned (m:1)\n");
	auto added = SavedTitle(MediaType::kManga, "Vagabond");
	CHECK(added.status == WatchStatus::kPlanToWatch);
	CHECK(added.total_units == 327);
	CHECK(added.total_volumes == 37);
	CHECK_FALSE(added.start_date);
	CHECK(Saved(MediaType::kAnime).empty());
}

TEST_CASE_FIXTURE(CliFixture, "cli add with several matches asks, or wants --pick") {
	Seed();
	web.Respond(R"("search":"fullmetal","type":"ANIME")", R"({"data":{"Page":{"media":[
	  {"id":5114,"idMal":5114,"title":{"romaji":"Fullmetal Alchemist: Brotherhood"},
	   "episodes":64,"startDate":{"year":2009}},
	  {"id":121,"idMal":121,"title":{"romaji":"Fullmetal Alchemist"},"episodes":51,
	   "startDate":{"year":2003}}]}}})");
	web.Respond(R"({"id":121,"type":"ANIME")",
	            R"({"data":{"Media":{"id":121,"idMal":121,"title":{"romaji":"Fullmetal Alchemist"},
	                "episodes":51,"status":"FINISHED"}}})");
	web.Respond(R"({"id":5114,"type":"ANIME")",
	            R"({"data":{"Media":{"id":5114,"idMal":5114,
	                "title":{"romaji":"Fullmetal Alchemist: Brotherhood"},"episodes":64}}})");

	CHECK(Run({"add", "fullmetal"}) == 1);
	CHECK(err ==
	      "error: 'fullmetal' matches 2 anime on AniList:\n"
	      "   1  2009  Fullmetal Alchemist: Brotherhood  (64 episodes)\n"
	      "   2  2003  Fullmetal Alchemist  (51 episodes)\n"
	      "Take one by its number with --pick=N.\n");
	CHECK(out.empty());
	CHECK(Saved(MediaType::kAnime).size() == 2);

	// At a terminal the person chooses; an empty answer cancels.
	CHECK(Run({"add", "fullmetal"}, "\n") == 1);
	CHECK(err.find("'fullmetal' matches 2 anime on AniList:\n") == 0);
	CHECK(Has(err, "Which one? [1-2, Enter to cancel]: Cancelled.\n"));
	CHECK(Saved(MediaType::kAnime).size() == 2);
	CHECK(Run({"add", "fullmetal"}, "2\n") == 0);
	CHECK(out == "a:3\tFullmetal Alchemist\tadded\t-\tplanned\n");

	CHECK(Run({"add", "--pick=1", "fullmetal"}) == 0);
	CHECK(out == "a:4\tFullmetal Alchemist: Brotherhood\tadded\t-\tplanned\n");
	CHECK(err.empty());
	CHECK(Saved(MediaType::kAnime).size() == 4);
	// --pick wins over the question.
	CHECK(Run({"add", "--pick", "2", "fullmetal"}, "1\n") == 0);
	CHECK(Has(err, "Fullmetal Alchemist is in your anime list already, as a:3 (planned).\n"));

	CHECK(Run({"add", "--pick=3", "fullmetal"}) == 1);
	CHECK(err == "error: --pick=3, but AniList has only 2 matches for 'fullmetal'\n");
}

TEST_CASE_FIXTURE(CliFixture, "cli add says what went wrong") {
	Seed();
	CHECK(Run({"add"}) == 2);
	CHECK(err.find("error: name the title to add\n"
	               "Usage: catalist add ") == 0);
	CHECK(Run({"add", ""}) == 2);
	CHECK(Run({"add", "odd", "--pick=1"}) == 2);
	CHECK(err.find("error: '--pick=1' comes after the text; options go before it\n") == 0);
	CHECK(Run({"add", "-am", "x"}) == 2);
	CHECK(err.find("error: give -a or -m, not both\n") == 0);
	CHECK(Run({"add", "--status", "nope", "x"}) == 2);
	CHECK(err.find("error: unknown status 'nope'\n") == 0);
	CHECK(Run({"add", "--pick", "0", "x"}) == 2);
	CHECK(Run({"add", "--pick", "two", "x"}) == 2);
	CHECK(err.find("error: --pick 'two' is not the number of a match\n") == 0);
	CHECK(Run({"add", "--no-lookup", "--pick=1", "x"}) == 2);
	// A wrong command line asks AniList nothing.
	CHECK(web.posted.empty());

	// The fake web answers nothing: offline.
	CHECK(Run({"add", "dandadan"}) == 1);
	CHECK(err ==
	      "error: could not reach AniList; check the connection and try again\n"
	      "To add a title without AniList: catalist add --no-lookup TITLE\n");
	web.Respond(R"("search":"zzz")", R"({"data":{"Page":{"media":[]}}})");
	CHECK(Run({"add", "-m", "zzz"}) == 1);
	CHECK(err == "error: AniList has no manga matching 'zzz'\n");
	// Found by the search, gone when looked up.
	web.Respond(R"("search":"ghost","type":"ANIME")", R"({"data":{"Page":{"media":[
	  {"id":999,"idMal":null,"title":{"romaji":"Ghost"}}]}}})");
	web.Respond(R"({"id":999,"type":"ANIME")", R"({"data":{"Media":null}})");
	CHECK(Run({"add", "ghost"}) == 1);
	CHECK(err == "error: AniList does not know the title\n");
	CHECK(out.empty());
	CHECK(Saved(MediaType::kAnime).size() == 2);
	CHECK(Steps().empty());
}

TEST_CASE_FIXTURE(CliFixture, "cli add --no-lookup adds a title as written, without AniList") {
	CHECK(Run({"add", "--no-lookup", "Home Video"}) == 0);
	CHECK(out == "a:1\tHome Video\tadded\t-\tplanned\n");
	CHECK(err.empty());
	CHECK(web.posted.empty());
	auto added = SavedTitle(MediaType::kAnime, "Home Video");
	CHECK(added.anilist_id == 0);
	CHECK(added.mal_id == 0);
	CHECK(added.total_units == 0);

	CHECK(Run({"add", "--no-lookup", "-m", "--status=reading", "Zine"}) == 0);
	CHECK(out == "m:1\tZine\tadded\t-\twatching\n");
	CHECK(catalist::FormatDate(SavedTitle(MediaType::kManga, "Zine").start_date) == "2026-10-20");

	// Nothing but the name tells two such titles apart: said, and added.
	CHECK(Run({"add", "--no-lookup", "home video"}) == 0);
	CHECK(err == "note: your anime list has a title of that name already, a:1.\n");
	CHECK(out == "a:2\thome video\tadded\t-\tplanned\n");
	CHECK(Saved(MediaType::kAnime).size() == 2);
	CHECK(web.posted.empty());
}

TEST_CASE_FIXTURE(CliFixture, "cli search prints the matches and which are yours") {
	Seed();
	web.Respond(R"("search":"frieren","type":"ANIME")", R"({"data":{"Page":{"media":[
	  {"id":154587,"idMal":52991,"title":{"romaji":"Sousou no Frieren"},"episodes":28,
	   "averageScore":89,"startDate":{"year":2023}},
	  {"id":182255,"idMal":59978,"title":{"romaji":"Sousou no Frieren 2nd Season"},
	   "startDate":{"year":2026}}]}}})");
	CHECK(Run({"search", "frieren"}) == 0);
	CHECK(out ==
	      "1\tanime\tSousou no Frieren\t2023\t28\t8.9\t154587\t52991\ta:1\t0\n"
	      "2\tanime\tSousou no Frieren 2nd Season\t2026\t0\t0\t182255\t59978\t-\t0\n");
	CHECK(err.empty());
	CHECK(Run({"search", "--header", "frieren"}) == 0);
	CHECK(
	    out.find(
	        "pick\ttype\ttitle\tyear\ttotal\tscore\tanilist_id\tmal_id\tref\tkitsu_id\n1\tanime\t") ==
	    0);
	CHECK(Run({"search", "--json", "frieren"}, "", 100) == 0);
	CHECK(
	    out ==
	    "{\"pick\":1,\"type\":\"anime\",\"title\":\"Sousou no Frieren\",\"year\":2023,"
	    "\"total\":28,\"score\":8.9,\"anilist_id\":154587,\"mal_id\":52991,\"ref\":\"a:1\",\"kitsu_id\":null}\n"
	    "{\"pick\":2,\"type\":\"anime\",\"title\":\"Sousou no Frieren 2nd Season\","
	    "\"year\":2026,\"total\":null,\"score\":null,\"anilist_id\":182255,\"mal_id\":59978,"
	    "\"ref\":null,\"kitsu_id\":null}\n");

	// A table at a terminal, with how to add one of them.
	CHECK(Run({"search", "frieren"}, "", 100) == 0);
	CHECK(out ==
	      "#  YEAR  TITLE                         EPISODES  SCORE  YOURS\n"
	      "1  2023  Sousou no Frieren                   28    8.9  a:1\n"
	      "2  2026  Sousou no Frieren 2nd Season         ?      -\n");
	CHECK(err == "Add one with: catalist add --pick=N QUERY\n");
	// Nothing is added, and no step is made.
	CHECK(Saved(MediaType::kAnime).size() == 2);
	CHECK(Steps().empty());
}

TEST_CASE_FIXTURE(CliFixture, "cli search -m searches manga") {
	Seed();
	web.Respond(R"("search":"berserk","type":"MANGA")", R"({"data":{"Page":{"media":[
	  {"id":30002,"idMal":2,"title":{"romaji":"Berserk"},"startDate":{"year":1989}}]}}})");
	CHECK(Run({"search", "-m", "berserk"}) == 0);
	CHECK(out == "1\tmanga\tBerserk\t1989\t0\t0\t30002\t2\tm:1\t0\n");
	CHECK(Run({"search", "-m", "berserk"}, "", 100) == 0);
	CHECK(Has(out, "CHAPTERS"));
	CHECK(err == "Add one with: catalist add -m --pick=N QUERY\n");
	// The anime of that name is another search.
	CHECK(Run({"search", "berserk"}) == 1);
}

TEST_CASE_FIXTURE(CliFixture, "cli search says what went wrong") {
	CHECK(Run({"search"}) == 2);
	CHECK(err.find("error: give the QUERY to search for\n"
	               "Usage: catalist search ") == 0);
	CHECK(Run({"search", "odd", "-m"}) == 2);
	CHECK(Run({"search", "-am", "taxi"}) == 2);
	CHECK(web.posted.empty());

	// The fake web answers nothing: offline.
	CHECK(Run({"search", "dandadan"}) == 1);
	CHECK(err == "error: could not reach AniList; check the connection and try again\n");
	CHECK(out.empty());

	// No match is not a failure.
	web.Respond(R"("search":"zzz")", R"({"data":{"Page":{"media":[]}}})");
	CHECK(Run({"search", "zzz"}) == 0);
	CHECK(out.empty());
	CHECK(err.empty());
	CHECK(Run({"search", "zzz"}, "", 100) == 0);
	CHECK(out.empty());
	CHECK(err == "AniList has no anime matching 'zzz'.\n");
	// The words of a query need no quotes.
	web.Respond(R"("search":"odd taxi")", R"({"data":{"Page":{"media":[]}}})");
	CHECK(Run({"search", "odd", "taxi"}, "", 100) == 0);
	CHECK(err == "AniList has no anime matching 'odd taxi'.\n");
	// Searching writes nothing, not even the data directory.
	CHECK_FALSE(std::filesystem::exists("data"));
}

TEST_CASE_FIXTURE(CliFixture, "cli add takes a title of several words without quotes") {
	Seed();
	CHECK(Run({"add", "--no-lookup", "Home", "Video"}) == 0);
	CHECK(out == "a:3\tHome Video\tadded\t-\tplanned\n");
	CHECK(Steps()[0].label == "add --no-lookup Home Video");
}

TEST_CASE_FIXTURE(CliFixture, "cli add takes a score and the progress made already") {
	Seed();
	// A progress starts the title.
	CHECK(Run({"add", "-q", "--no-lookup", "--progress=3", "--score=7", "Home Video"}) == 0);
	auto added = SavedTitle(MediaType::kAnime, "Home Video");
	CHECK(added.status == WatchStatus::kWatching);
	CHECK(added.progress == 3);
	CHECK(added.score == 7);
	CHECK(catalist::FormatDate(added.start_date) == "2026-10-20");
	// A status that is given stands.
	CHECK(Run({"add", "-q", "--no-lookup", "--progress=2", "--status=on-hold", "Ping Pong"}) == 0);
	auto held = SavedTitle(MediaType::kAnime, "Ping Pong");
	CHECK(held.status == WatchStatus::kOnHold);
	CHECK(held.progress == 2);
	// One step for undo, as any add.
	CHECK(Steps().size() == 2);

	CHECK(Run({"add", "--no-lookup", "--score=11", "x"}) == 2);
	CHECK(err.find("error: --score '11' is not 0 to 10\n") == 0);
	CHECK(Run({"add", "--no-lookup", "--progress=lots", "x"}) == 2);
	CHECK(err.find("error: --progress 'lots' is not a count\n") == 0);
}

TEST_CASE_FIXTURE(CliFixture, "cli search and add ask the site --from names, or the provider") {
	Seed();
	const char* frieren = R"({"data":[{"id":"46474","type":"anime","attributes":{
	  "canonicalTitle":"Sousou no Frieren","episodeCount":28,"startDate":"2023-09-29",
	  "averageRating":"89.6","status":"finished"},
	  "relationships":{"mappings":{"data":[{"type":"mappings","id":"7"}]}}}],
	  "included":[{"id":"7","type":"mappings",
	  "attributes":{"externalSite":"myanimelist/anime","externalId":"52991"}}]})";
	web.Respond("GET https://kitsu.app/api/edge/anime?filter[text]=frieren", frieren);
	CHECK(Run({"search", "--from", "kitsu", "frieren"}) == 0);
	// Kitsu's mapping says which title of the lists it is.
	CHECK(out == "1\tanime\tSousou no Frieren\t2023\t28\t9.0\t0\t52991\ta:1\t46474\n");
	CHECK(web.posted.empty());

	// With Kitsu as the provider, --from is not needed.
	CHECK(Run({"provider", "kitsu"}) == 0);
	CHECK(Run({"search", "frieren"}, "", 100) == 0);
	CHECK(Has(err, "Add one with: catalist add --pick=N QUERY\n"));

	// A title Kitsu does not have.
	web.Respond("GET https://kitsu.app/api/edge/anime?filter[text]=zzz", R"({"data":[]})");
	CHECK(Run({"add", "zzz"}) == 1);
	CHECK(err == "error: Kitsu has no anime matching 'zzz'\n");

	CHECK(Run({"search", "--from=shikimori", "frieren"}) == 2);
	CHECK(err.find("error: --from 'shikimori' is not anilist, mal or kitsu\n") == 0);
	CHECK(Run({"add", "--no-lookup", "--from=mal", "x"}) == 2);
	CHECK(err.find("error: --from chooses among a site's matches; --no-lookup asks none\n") == 0);
}

TEST_CASE_FIXTURE(CliFixture, "cli add from Kitsu keeps every id Kitsu knows") {
	Seed();
	web.Respond("GET https://kitsu.app/api/edge/anime?filter[text]=dandadan",
	            R"({"data":[{"id":"48291","type":"anime","attributes":{"canonicalTitle":"Dandadan"},
	            "relationships":{"mappings":{"data":[]}}}]})");
	web.Respond("GET https://kitsu.app/api/edge/anime/48291?include=mappings",
	            R"({"data":{"id":"48291","type":"anime","attributes":{"canonicalTitle":"Dandadan",
	            "episodeCount":12,"status":"finished"},
	            "relationships":{"mappings":{"data":[{"type":"mappings","id":"1"},
	            {"type":"mappings","id":"2"}]}}},"included":[
	            {"id":"1","type":"mappings","attributes":{"externalSite":"myanimelist/anime","externalId":"57334"}},
	            {"id":"2","type":"mappings","attributes":{"externalSite":"anilist/anime","externalId":"171018"}}]})");
	CHECK(Run({"add", "--from", "kitsu", "dandadan"}) == 0);
	auto added = SavedTitle(MediaType::kAnime, "Dandadan");
	CHECK(added.kitsu_id == 48291);
	CHECK(added.mal_id == 57334);
	CHECK(added.anilist_id == 171018);
	CHECK(added.total_units == 12);
	// Known by any of its ids: AniList's answer finds it in the list.
	CHECK(Run({"show", "--json", "dandadan"}) == 0);
	CHECK(Has(out, "\"kitsu_id\":48291}"));
}

TEST_CASE_FIXTURE(CliFixture, "cli says what MyAnimeList needs before it can be asked") {
	CHECK(Run({"search", "--from", "mal", "frieren"}) == 1);
	CHECK(Has(err, "error: MyAnimeList answers only programs with a client id"));
	CHECK(web.sent.empty());
}

TEST_CASE_FIXTURE(CliFixture, "cli refresh asks what is out of the titles being followed") {
	Seed();
	web.Respond(R"("ids":[128547],"type":"ANIME")",
	            R"({"data":{"Page":{"media":[{"id":128547,"idMal":44265,"episodes":13,
	            "status":"FINISHED"}]}}})");
	web.Respond(R"("ids":[30002],"type":"MANGA")",
	            R"({"data":{"Page":{"media":[{"id":30002,"idMal":2,"chapters":380,
	            "volumes":42,"status":"FINISHED"}]}}})");
	CHECK(Run({"refresh"}) == 0);
	// Only a total a person sees is a change; what is out is kept quietly.
	CHECK(out == "m:1\tBerserk\ttotal\t0\t380\n");
	CHECK(err == "Checked 2 titles on AniList; 2 have new episodes or chapters out.\n");
	// One request for each list.
	CHECK(web.sent.size() == 2);
	auto berserk = SavedTitle(MediaType::kManga, "Berserk");
	CHECK(berserk.total_units == 380);
	CHECK(berserk.released == 380);
	CHECK(berserk.total_volumes == 42);
	REQUIRE(Steps().size() == 1);
	CHECK(Steps()[0].label == "refresh");

	// Asked again, nothing a person sees changes: no step for undo.
	CHECK(Run({"refresh", "-q"}) == 0);
	CHECK(out.empty());
	CHECK(err.empty());
	CHECK(Steps().size() == 1);
}

TEST_CASE_FIXTURE(CliFixture, "cli refresh takes REFs, and says what it could not find") {
	Seed();
	CHECK(Run({"add", "-q", "--no-lookup", "--status=watching", "Home Video"}) == 0);
	web.Respond(R"("ids":[154587],"type":"ANIME")",
	            R"({"data":{"Page":{"media":[{"id":154587,"idMal":52991,"episodes":28,
	            "status":"FINISHED"}]}}})");
	CHECK(Run({"refresh", "a:1", "home"}) == 0);
	CHECK(err ==
	      "note: AniList could not find a:3.\n"
	      "Checked 1 title on AniList.\n");

	// Offline: nothing is saved.
	CHECK(Run({"refresh", "taxi"}) == 1);
	CHECK(err == "error: could not reach AniList; check the connection and try again\n");
	CHECK(SavedTitle(MediaType::kAnime, "Odd Taxi").checked_at ==
	      catalist::test::RequireDate("2026-10-20"));
}

TEST_CASE_FIXTURE(CliFixture, "cli refresh with nothing followed says what to give it") {
	CHECK(Run({"refresh"}) == 0);
	CHECK(err == "Nothing is being watched or read: name the titles to refresh.\n");
	CHECK(web.sent.empty());
}

TEST_CASE_FIXTURE(CliFixture, "cli searches by a site's id, and in your own list there") {
	Seed();
	web.Respond(R"({"id":171018,"type":"ANIME")", R"({"data":{"Media":{"id":171018,
	  "idMal":57334,"title":{"romaji":"Dandadan"},"episodes":12,"status":"FINISHED"}}})");
	// id:N is the title with that id on the site.
	CHECK(Run({"search", "id:171018"}) == 0);
	CHECK(out == "1\tanime\tDandadan\t-\t12\t0\t171018\t57334\t-\t0\n");
	CHECK(Run({"add", "id:171018"}) == 0);
	CHECK(SavedTitle(MediaType::kAnime, "Dandadan").anilist_id == 171018);
	CHECK(Run({"search", "id:x"}) == 1);
	CHECK(Has(err, "'id:x' is not id: and a number"));

	// my: is your own list there, which wants a login.
	CHECK(Run({"search", "my:taxi"}) == 1);
	CHECK(Has(err, "my: searches your list on AniList: log in first"));
	LogIn(catalist::Provider::kAniList);
	web.Respond(R"("u":"Abel")", R"({"data":{"MediaListCollection":{"lists":[{"entries":[
	  {"status":"CURRENT","score":8,"progress":5,"media":{"id":128547,"idMal":44265,
	   "title":{"romaji":"Odd Taxi"},"episodes":13,"status":"FINISHED"}},
	  {"status":"PLANNING","score":0,"progress":0,"media":{"id":1,"idMal":1,
	   "title":{"romaji":"Cowboy Bebop"},"episodes":26,"status":"FINISHED"}}]}]}}})");
	CHECK(Run({"search", "my:taxi"}) == 0);
	CHECK(out == "1\tanime\tOdd Taxi\t-\t13\t0\t128547\t44265\ta:2\t0\n");
}
