#include <doctest/doctest.h>

#include <filesystem>

#include "support/cliFixture.hpp"

using catalist::mediaType;
using catalist::watchStatus;
using catalist::test::cliFixture;
using catalist::test::has;

TEST_CASE_FIXTURE(cliFixture, "cli add finds the title on AniList and adds it") {
	seed();
	web.respond(R"("search":"dandadan","type":"ANIME")", R"({"data":{"Page":{"media":[
	  {"id":171018,"idMal":57334,"title":{"romaji":"Dandadan"},"episodes":12,
	   "startDate":{"year":2024}}]}}})");
	web.respond(R"({"id":171018,"type":"ANIME")", R"({"data":{"Media":{"id":171018,
	  "idMal":57334,"title":{"romaji":"Dandadan"},"episodes":12,"status":"FINISHED"}}})");
	CHECK(run({"add", "--status", "watching", "dandadan"}) == 0);
	CHECK(out == "a:3\tDandadan\tadded\t-\twatching\n");
	CHECK(err.empty());
	auto added = savedTitle(mediaType::kAnime, "Dandadan");
	CHECK(added.anilistId == 171018);
	CHECK(added.malId == 57334);
	CHECK(added.totalUnits == 12);
	CHECK(added.released == 12);
	CHECK(added.progress == 0);
	CHECK(catalist::formatDate(added.startDate) == "2026-10-20");
	CHECK(catalist::formatDate(added.updatedAt) == "2026-10-20");

	// Once is enough.
	CHECK(run({"add", "dandadan"}) == 0);
	CHECK(err == "Dandadan is in your anime list already, as a:3 (watching).\nNothing changed.\n");
	CHECK(out.empty());
	CHECK(saved(mediaType::kAnime).size() == 3);

	// One step for undo, and the ref is free again.
	REQUIRE(steps().size() == 1);
	CHECK(steps()[0].label == "add --status watching dandadan");
	CHECK(run({"undo"}) == 0);
	CHECK(saved(mediaType::kAnime).size() == 2);
	CHECK(run({"add", "-q", "--status=completed", "dandadan"}) == 0);
	CHECK(out.empty());
	added = savedTitle(mediaType::kAnime, "Dandadan");
	CHECK(added.id == 3);
	CHECK(added.status == watchStatus::kCompleted);
	CHECK(added.progress == 12);
}

TEST_CASE_FIXTURE(cliFixture, "cli add -m adds a manga, planned unless told otherwise") {
	web.respond(R"("search":"vagabond","type":"MANGA")", R"({"data":{"Page":{"media":[
	  {"id":30656,"idMal":656,"title":{"romaji":"Vagabond"},"chapters":327,
	   "startDate":{"year":1998}}]}}})");
	web.respond(R"({"id":30656,"type":"MANGA")", R"({"data":{"Media":{"id":30656,
	  "idMal":656,"title":{"romaji":"Vagabond"},"chapters":327,"volumes":37,
	  "status":"HIATUS"}}})");
	CHECK(run({"add", "-m", "vagabond"}, "", 100) == 0);
	CHECK(out == "Vagabond  added as planned (m:1)\n");
	auto added = savedTitle(mediaType::kManga, "Vagabond");
	CHECK(added.status == watchStatus::kPlanToWatch);
	CHECK(added.totalUnits == 327);
	CHECK(added.totalVolumes == 37);
	CHECK_FALSE(added.startDate);
	CHECK(saved(mediaType::kAnime).empty());
}

TEST_CASE_FIXTURE(cliFixture, "cli add with several matches asks, or wants --pick") {
	seed();
	web.respond(R"("search":"fullmetal","type":"ANIME")", R"({"data":{"Page":{"media":[
	  {"id":5114,"idMal":5114,"title":{"romaji":"Fullmetal Alchemist: Brotherhood"},
	   "episodes":64,"startDate":{"year":2009}},
	  {"id":121,"idMal":121,"title":{"romaji":"Fullmetal Alchemist"},"episodes":51,
	   "startDate":{"year":2003}}]}}})");
	web.respond(R"({"id":121,"type":"ANIME")",
	            R"({"data":{"Media":{"id":121,"idMal":121,"title":{"romaji":"Fullmetal Alchemist"},
	                "episodes":51,"status":"FINISHED"}}})");
	web.respond(R"({"id":5114,"type":"ANIME")",
	            R"({"data":{"Media":{"id":5114,"idMal":5114,
	                "title":{"romaji":"Fullmetal Alchemist: Brotherhood"},"episodes":64}}})");

	CHECK(run({"add", "fullmetal"}) == 1);
	CHECK(err ==
	      "error: 'fullmetal' matches 2 anime on AniList:\n"
	      "   1  2009  Fullmetal Alchemist: Brotherhood  (64 episodes)\n"
	      "   2  2003  Fullmetal Alchemist  (51 episodes)\n"
	      "Take one by its number with --pick=N.\n");
	CHECK(out.empty());
	CHECK(saved(mediaType::kAnime).size() == 2);

	// At a terminal the person chooses; an empty answer cancels.
	CHECK(run({"add", "fullmetal"}, "\n") == 1);
	CHECK(err.find("'fullmetal' matches 2 anime on AniList:\n") == 0);
	CHECK(has(err, "Which one? [1-2, Enter to cancel]: Cancelled.\n"));
	CHECK(saved(mediaType::kAnime).size() == 2);
	CHECK(run({"add", "fullmetal"}, "2\n") == 0);
	CHECK(out == "a:3\tFullmetal Alchemist\tadded\t-\tplanned\n");

	CHECK(run({"add", "--pick=1", "fullmetal"}) == 0);
	CHECK(out == "a:4\tFullmetal Alchemist: Brotherhood\tadded\t-\tplanned\n");
	CHECK(err.empty());
	CHECK(saved(mediaType::kAnime).size() == 4);
	// --pick wins over the question.
	CHECK(run({"add", "--pick", "2", "fullmetal"}, "1\n") == 0);
	CHECK(has(err, "Fullmetal Alchemist is in your anime list already, as a:3 (planned).\n"));

	CHECK(run({"add", "--pick=3", "fullmetal"}) == 1);
	CHECK(err == "error: --pick=3, but AniList has only 2 matches for 'fullmetal'\n");
}

TEST_CASE_FIXTURE(cliFixture, "cli add says what went wrong") {
	seed();
	CHECK(run({"add"}) == 2);
	CHECK(err.find("error: name the title to add\n"
	               "Usage: catalist add ") == 0);
	CHECK(run({"add", ""}) == 2);
	CHECK(run({"add", "odd", "--pick=1"}) == 2);
	CHECK(err.find("error: '--pick=1' comes after the text; options go before it\n") == 0);
	CHECK(run({"add", "-am", "x"}) == 2);
	CHECK(err.find("error: give -a or -m, not both\n") == 0);
	CHECK(run({"add", "--status", "nope", "x"}) == 2);
	CHECK(err.find("error: unknown status 'nope'\n") == 0);
	CHECK(run({"add", "--pick", "0", "x"}) == 2);
	CHECK(run({"add", "--pick", "two", "x"}) == 2);
	CHECK(err.find("error: --pick 'two' is not the number of a match\n") == 0);
	CHECK(run({"add", "--no-lookup", "--pick=1", "x"}) == 2);
	// A wrong command line asks AniList nothing.
	CHECK(web.posted.empty());

	// The fake web answers nothing: offline.
	CHECK(run({"add", "dandadan"}) == 1);
	CHECK(err ==
	      "error: could not reach AniList; check the connection and try again\n"
	      "To add a title without AniList: catalist add --no-lookup TITLE\n");
	web.respond(R"("search":"zzz")", R"({"data":{"Page":{"media":[]}}})");
	CHECK(run({"add", "-m", "zzz"}) == 1);
	CHECK(err == "error: AniList has no manga matching 'zzz'\n");
	// Found by the search, gone when looked up.
	web.respond(R"("search":"ghost","type":"ANIME")", R"({"data":{"Page":{"media":[
	  {"id":999,"idMal":null,"title":{"romaji":"Ghost"}}]}}})");
	web.respond(R"({"id":999,"type":"ANIME")", R"({"data":{"Media":null}})");
	CHECK(run({"add", "ghost"}) == 1);
	CHECK(err == "error: AniList does not know the title\n");
	CHECK(out.empty());
	CHECK(saved(mediaType::kAnime).size() == 2);
	CHECK(steps().empty());
}

TEST_CASE_FIXTURE(cliFixture, "cli add --no-lookup adds a title as written, without AniList") {
	CHECK(run({"add", "--no-lookup", "Home Video"}) == 0);
	CHECK(out == "a:1\tHome Video\tadded\t-\tplanned\n");
	CHECK(err.empty());
	CHECK(web.posted.empty());
	auto added = savedTitle(mediaType::kAnime, "Home Video");
	CHECK(added.anilistId == 0);
	CHECK(added.malId == 0);
	CHECK(added.totalUnits == 0);

	CHECK(run({"add", "--no-lookup", "-m", "--status=reading", "Zine"}) == 0);
	CHECK(out == "m:1\tZine\tadded\t-\twatching\n");
	CHECK(catalist::formatDate(savedTitle(mediaType::kManga, "Zine").startDate) == "2026-10-20");

	// Nothing but the name tells two such titles apart: said, and added.
	CHECK(run({"add", "--no-lookup", "home video"}) == 0);
	CHECK(err == "note: your anime list has a title of that name already, a:1.\n");
	CHECK(out == "a:2\thome video\tadded\t-\tplanned\n");
	CHECK(saved(mediaType::kAnime).size() == 2);
	CHECK(web.posted.empty());
}

TEST_CASE_FIXTURE(cliFixture, "cli search prints the matches and which are yours") {
	seed();
	web.respond(R"("search":"frieren","type":"ANIME")", R"({"data":{"Page":{"media":[
	  {"id":154587,"idMal":52991,"title":{"romaji":"Sousou no Frieren"},"episodes":28,
	   "averageScore":89,"startDate":{"year":2023}},
	  {"id":182255,"idMal":59978,"title":{"romaji":"Sousou no Frieren 2nd Season"},
	   "startDate":{"year":2026}}]}}})");
	CHECK(run({"search", "frieren"}) == 0);
	CHECK(out ==
	      "1\tanime\tSousou no Frieren\t2023\t28\t8.9\t154587\t52991\ta:1\n"
	      "2\tanime\tSousou no Frieren 2nd Season\t2026\t0\t0\t182255\t59978\t-\n");
	CHECK(err.empty());
	CHECK(run({"search", "--header", "frieren"}) == 0);
	CHECK(out.find("pick\ttype\ttitle\tyear\ttotal\tscore\tanilist_id\tmal_id\tref\n1\tanime\t") ==
	      0);
	CHECK(run({"search", "--json", "frieren"}, "", 100) == 0);
	CHECK(out ==
	      "{\"pick\":1,\"type\":\"anime\",\"title\":\"Sousou no Frieren\",\"year\":2023,"
	      "\"total\":28,\"score\":8.9,\"anilist_id\":154587,\"mal_id\":52991,\"ref\":\"a:1\"}\n"
	      "{\"pick\":2,\"type\":\"anime\",\"title\":\"Sousou no Frieren 2nd Season\","
	      "\"year\":2026,\"total\":null,\"score\":null,\"anilist_id\":182255,\"mal_id\":59978,"
	      "\"ref\":null}\n");

	// A table at a terminal, with how to add one of them.
	CHECK(run({"search", "frieren"}, "", 100) == 0);
	CHECK(out ==
	      "#  YEAR  TITLE                         EPISODES  SCORE  YOURS\n"
	      "1  2023  Sousou no Frieren                   28    8.9  a:1\n"
	      "2  2026  Sousou no Frieren 2nd Season         ?      -\n");
	CHECK(err == "Add one with: catalist add --pick=N QUERY\n");
	// Nothing is added, and no step is made.
	CHECK(saved(mediaType::kAnime).size() == 2);
	CHECK(steps().empty());
}

TEST_CASE_FIXTURE(cliFixture, "cli search -m searches manga") {
	seed();
	web.respond(R"("search":"berserk","type":"MANGA")", R"({"data":{"Page":{"media":[
	  {"id":30002,"idMal":2,"title":{"romaji":"Berserk"},"startDate":{"year":1989}}]}}})");
	CHECK(run({"search", "-m", "berserk"}) == 0);
	CHECK(out == "1\tmanga\tBerserk\t1989\t0\t0\t30002\t2\tm:1\n");
	CHECK(run({"search", "-m", "berserk"}, "", 100) == 0);
	CHECK(has(out, "CHAPTERS"));
	CHECK(err == "Add one with: catalist add -m --pick=N QUERY\n");
	// The anime of that name is another search.
	CHECK(run({"search", "berserk"}) == 1);
}

TEST_CASE_FIXTURE(cliFixture, "cli search says what went wrong") {
	CHECK(run({"search"}) == 2);
	CHECK(err.find("error: give the QUERY to search for\n"
	               "Usage: catalist search ") == 0);
	CHECK(run({"search", "odd", "-m"}) == 2);
	CHECK(run({"search", "-am", "taxi"}) == 2);
	CHECK(web.posted.empty());

	// The fake web answers nothing: offline.
	CHECK(run({"search", "dandadan"}) == 1);
	CHECK(err == "error: could not reach AniList; check the connection and try again\n");
	CHECK(out.empty());

	// No match is not a failure.
	web.respond(R"("search":"zzz")", R"({"data":{"Page":{"media":[]}}})");
	CHECK(run({"search", "zzz"}) == 0);
	CHECK(out.empty());
	CHECK(err.empty());
	CHECK(run({"search", "zzz"}, "", 100) == 0);
	CHECK(out.empty());
	CHECK(err == "AniList has no anime matching 'zzz'.\n");
	// The words of a query need no quotes.
	web.respond(R"("search":"odd taxi")", R"({"data":{"Page":{"media":[]}}})");
	CHECK(run({"search", "odd", "taxi"}, "", 100) == 0);
	CHECK(err == "AniList has no anime matching 'odd taxi'.\n");
	// Searching writes nothing, not even the data directory.
	CHECK_FALSE(std::filesystem::exists("data"));
}

TEST_CASE_FIXTURE(cliFixture, "cli add takes a title of several words without quotes") {
	seed();
	CHECK(run({"add", "--no-lookup", "Home", "Video"}) == 0);
	CHECK(out == "a:3\tHome Video\tadded\t-\tplanned\n");
	CHECK(steps()[0].label == "add --no-lookup Home Video");
}
