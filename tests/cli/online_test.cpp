#include <doctest/doctest.h>

#include <filesystem>

#include "support/cli_fixture.hpp"

using catalist::MediaType;
using catalist::WatchStatus;
using catalist::test::CliFixture;
using catalist::test::Has;

TEST_CASE_FIXTURE(CliFixture, "cli add finds the title on AniList and adds it") {
	Seed();
	web.Respond(R"("search":"dandadan")", R"({"data":{"anime":{"media":[
	  {"id":171018,"idMal":57334,"title":{"romaji":"Dandadan"},"episodes":12,
	   "startDate":{"year":2024}}]}}})");
	CHECK(Run({"add", "--status", "watching", "dandadan"}) == 0);
	CHECK(out == "a:3\tDandadan\tadded\t-\twatching\n");
	CHECK(err.empty());
	CHECK(web.posted.size() == 1);
	auto added = SavedTitle(MediaType::kAnime, "Dandadan");
	CHECK(added.anilist_id == 171018);
	CHECK(added.mal_id == 57334);
	CHECK(added.total_units == 12);
	CHECK(added.progress == 0);
	CHECK(catalist::FormatDate(added.start_date) == "2026-10-20");
	CHECK(catalist::FormatDate(added.updated_at) == "2026-10-20");

	CHECK(Run({"add", "dandadan"}) == 0);
	CHECK(err == "Dandadan is in your anime list already (watching).\nNothing changed.\n");
	CHECK(out.empty());
	CHECK(Saved(MediaType::kAnime).size() == 3);

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
	web.Respond(R"("search":"vagabond")", R"({"data":{"manga":{"media":[
	  {"id":30656,"idMal":656,"title":{"romaji":"Vagabond"},"chapters":327,"volumes":37,
	   "startDate":{"year":1998}}]}}})");
	CHECK(Run({"add", "-m", "vagabond"}, "", 100) == 0);
	CHECK(out == "Vagabond  added as planned\n");
	auto added = SavedTitle(MediaType::kManga, "Vagabond");
	CHECK(added.status == WatchStatus::kPlanToWatch);
	CHECK(added.total_units == 327);
	CHECK(added.total_volumes == 37);
	CHECK_FALSE(added.start_date);
	CHECK(Saved(MediaType::kAnime).empty());
}

TEST_CASE_FIXTURE(CliFixture, "cli add with several matches asks, or wants --pick") {
	Seed();
	web.Respond(R"("search":"fullmetal")", R"({"data":{"anime":{"media":[
	  {"id":5114,"idMal":5114,"title":{"romaji":"Fullmetal Alchemist: Brotherhood"},
	   "episodes":64,"startDate":{"year":2009}},
	  {"id":121,"idMal":121,"title":{"romaji":"Fullmetal Alchemist"},"episodes":51,
	   "startDate":{"year":2003}}]}}})");

	CHECK(Run({"add", "fullmetal"}) == 1);
	CHECK(err ==
	      "error: 'fullmetal' matches 2 titles on AniList:\n"
	      "ANIME\n"
	      "   1  2009  Fullmetal Alchemist: Brotherhood  (64 episodes)\n"
	      "   2  2003  Fullmetal Alchemist  (51 episodes)\n"
	      "Take one by its number with --pick=N.\n");
	CHECK(out.empty());
	CHECK(Saved(MediaType::kAnime).size() == 2);

	CHECK(Run({"add", "fullmetal"}, "\n") == 1);
	CHECK(err.find("'fullmetal' matches 2 titles on AniList:\n") == 0);
	CHECK(Has(err, "Which one? [1-2, Enter to cancel]: Cancelled.\n"));
	CHECK(Saved(MediaType::kAnime).size() == 2);
	CHECK(Run({"add", "fullmetal"}, "2\n") == 0);
	CHECK(out == "a:3\tFullmetal Alchemist\tadded\t-\tplanned\n");

	CHECK(Run({"add", "--pick=1", "fullmetal"}) == 0);
	CHECK(out == "a:4\tFullmetal Alchemist: Brotherhood\tadded\t-\tplanned\n");
	CHECK(err.empty());
	CHECK(Saved(MediaType::kAnime).size() == 4);
	CHECK(Run({"add", "--pick", "2", "fullmetal"}, "1\n") == 0);
	CHECK(Has(err, "Fullmetal Alchemist is in your anime list already (planned).\n"));

	CHECK(Run({"add", "--pick=3", "fullmetal"}) == 1);
	CHECK(err == "error: --pick=3, but AniList has only 2 matches for 'fullmetal'\n");
}

TEST_CASE_FIXTURE(CliFixture, "cli add says what went wrong") {
	Seed();
	CHECK(Run({"add"}) == 2);
	CHECK(err.find("error: name the title to add\n"
	               "Usage: catalist add ") == 0);
	CHECK(Run({"add", ""}) == 2);
	CHECK(Run({"add", "-am", "x"}) == 2);
	CHECK(err.find("error: give -a or -m, not both\n") == 0);
	CHECK(Run({"add", "--status", "nope", "x"}) == 2);
	CHECK(err.find("error: unknown status 'nope'\n") == 0);
	CHECK(Run({"add", "--pick", "0", "x"}) == 2);
	CHECK(Run({"add", "--pick", "two", "x"}) == 2);
	CHECK(err.find("error: --pick 'two' is not the number of a match\n") == 0);
	CHECK(web.posted.empty());

	// The fake web answers nothing: offline.
	CHECK(Run({"add", "dandadan"}) == 1);
	CHECK(err ==
	      "error: could not reach AniList; check the connection and try again\n");
	web.Respond(R"("search":"zzz")", R"({"data":{"anime":{"media":[]}}})");
	CHECK(Run({"add", "-m", "zzz"}) == 1);
	CHECK(err == "error: AniList has no manga matching 'zzz'\n");
	CHECK(out.empty());
	CHECK(Saved(MediaType::kAnime).size() == 2);
	CHECK(Steps().empty());
}

TEST_CASE_FIXTURE(CliFixture, "cli add has no way around AniList") {
	CHECK(Run({"add", "--no-lookup", "Home Video"}) == 2);
	CHECK(err.find("error: unknown option '--no-lookup'\n") == 0);
	CHECK_FALSE(std::filesystem::exists("data"));
}

TEST_CASE_FIXTURE(CliFixture, "cli add looks at both types, numbered straight through") {
	web.Respond(R"("search":"berserk")", R"({"data":{
	  "anime":{"media":[{"id":33,"idMal":33,"title":{"romaji":"Kenpuu Denki Berserk",
	    "english":"Berserk"},"episodes":25,"startDate":{"year":1997}}]},
	  "manga":{"media":[{"id":30002,"idMal":2,"title":{"romaji":"Berserk"},
	    "startDate":{"year":1989}}]}}})");
	CHECK(Run({"add", "--pick=2", "berserk"}) == 0);
	CHECK(out == "m:1\tBerserk\tadded\t-\tplanned\n");
	CHECK(Run({"undo", "-q"}) == 0);

	CHECK(Run({"add", "berserk"}, "1\n") == 0);
	CHECK(err ==
	      "'berserk' matches 2 titles on AniList:\n"
	      "ANIME\n"
	      "   1  1997  Berserk  (25 episodes)\n"
	      "MANGA\n"
	      "   2  1989  Berserk  (? chapters)\n"
	      "Which one? [1-2, Enter to cancel]: ");
	auto added = Saved(MediaType::kAnime)[0];
	CHECK(added.title == "Kenpuu Denki Berserk");
	CHECK(added.english_title == "Berserk");
}

TEST_CASE_FIXTURE(CliFixture, "cli add takes the one exact name, only at a terminal") {
	web.Respond(R"("search":"frieren beyond journeys end")", R"({"data":{"anime":{"media":[
	  {"id":154587,"idMal":52991,"title":{"romaji":"Sousou no Frieren",
	   "english":"Frieren: Beyond Journey's End"},"episodes":28},
	  {"id":182255,"idMal":59978,"title":{"romaji":"Sousou no Frieren 2nd Season"}}]},
	  "manga":{"media":[]}}})");
	CHECK(Run({"add", "frieren", "beyond", "journeys", "end"}) == 1);
	CHECK(Has(err, "Take one by its number with --pick=N.\n"));
	CHECK(Run({"add", "frieren", "beyond", "journeys", "end"}, "unused\n") == 0);
	CHECK(SavedTitle(MediaType::kAnime, "Frieren: Beyond Journey's End").mal_id == 52991);
}

TEST_CASE_FIXTURE(CliFixture, "cli add takes several titles as one step, or none") {
	KnowTitle("Odd Taxi", 44265);
	KnowTitle("Dandadan", 57334);
	CHECK(Run({"add", "Odd", "Taxi,", "Dandadan"}) == 0);
	CHECK(Saved(MediaType::kAnime).size() == 2);
	CHECK(Steps().size() == 1);
	CHECK(Steps()[0].changes == 2);
	CHECK(Run({"undo", "-q"}) == 0);
	CHECK(Saved(MediaType::kAnime).empty());

	CHECK(Run({"add", "Odd", "Taxi,", "Unknown"}) == 1);
	CHECK(Saved(MediaType::kAnime).empty());
	CHECK(Run({"add", "--pick=1", "Odd", "Taxi,", "Dandadan"}) == 2);
	CHECK(Has(err, "--pick takes one title at a time"));

	CHECK(Run({"add", "Odd", "Taxi,", "Odd", "Taxi"}) == 0);
	CHECK(Saved(MediaType::kAnime).size() == 1);
	CHECK(Has(err, "Odd Taxi is in your anime list already (planned).\n"));
}

TEST_CASE_FIXTURE(CliFixture, "cli search prints the matches and which are yours") {
	Seed();
	web.Respond(R"("search":"frieren")", R"({"data":{"anime":{"media":[
	  {"id":154587,"idMal":52991,"title":{"romaji":"Sousou no Frieren"},"episodes":28,
	   "averageScore":89,"startDate":{"year":2023}},
	  {"id":182255,"idMal":59978,"title":{"romaji":"Sousou no Frieren 2nd Season"},
	   "startDate":{"year":2026}}]}}})");
	CHECK(Run({"search", "frieren"}) == 0);
	CHECK(out ==
	      "1\tanime\tSousou no Frieren\t2023\t28\t8.9\t154587\t52991\ta:1\n"
	      "2\tanime\tSousou no Frieren 2nd Season\t2026\t0\t0\t182255\t59978\t-\n");
	CHECK(err.empty());

	CHECK(Run({"search", "frieren"}, "", 100) == 0);
	CHECK(out ==
	      "ANIME\n"
	      "1  2023  Sousou no Frieren             28 episodes  "
	      "\U000F04CE\U000F04CE\U000F04CE\U000F04CE\U000F04D0  Completed\n"
	      "2  2026  Sousou no Frieren 2nd Season   ? episodes  -\n");
	CHECK(err == "Add one with: catalist add --pick=N QUERY\n");
	CHECK(Saved(MediaType::kAnime).size() == 2);
	CHECK(Steps().empty());
}

TEST_CASE_FIXTURE(CliFixture, "cli search -m searches manga") {
	Seed();
	web.Respond(R"("search":"berserk")", R"({"data":{"manga":{"media":[
	  {"id":30002,"idMal":2,"title":{"romaji":"Berserk"},"startDate":{"year":1989}}]}}})");
	CHECK(Run({"search", "-m", "berserk"}) == 0);
	CHECK(out == "1\tmanga\tBerserk\t1989\t0\t0\t30002\t2\tm:1\n");
	CHECK(Run({"search", "-m", "berserk"}, "", 100) == 0);
	CHECK(Has(out, "MANGA\n"));
	CHECK(Has(out, "? chapters"));
	CHECK(err == "Add one with: catalist add -m --pick=N QUERY\n");
}

TEST_CASE_FIXTURE(CliFixture, "cli search says what went wrong") {
	CHECK(Run({"search"}) == 2);
	CHECK(err.find("error: give the QUERY to search for\n"
	               "Usage: catalist search ") == 0);
	CHECK(Run({"search", "-am", "taxi"}) == 2);
	CHECK(web.posted.empty());

	// The fake web answers nothing: offline.
	CHECK(Run({"search", "dandadan"}) == 1);
	CHECK(err == "error: could not reach AniList; check the connection and try again\n");
	CHECK(out.empty());

	web.Respond(R"("search":"zzz")", R"({"data":{"anime":{"media":[]}}})");
	CHECK(Run({"search", "zzz"}) == 0);
	CHECK(out.empty());
	CHECK(err.empty());
	CHECK(Run({"search", "zzz"}, "", 100) == 0);
	CHECK(out.empty());
	CHECK(err == "AniList has no title matching 'zzz'.\n");
	web.Respond(R"("search":"odd taxi")", R"({"data":{"anime":{"media":[]}}})");
	CHECK(Run({"search", "odd", "taxi"}, "", 100) == 0);
	CHECK(err == "AniList has no title matching 'odd taxi'.\n");
	CHECK_FALSE(std::filesystem::exists("data"));
}

TEST_CASE_FIXTURE(CliFixture, "cli add takes a title of several words without quotes") {
	Seed();
	KnowTitle("Home Video", 900);
	CHECK(Run({"add", "Home", "Video"}) == 0);
	CHECK(out == "a:3\tHome Video\tadded\t-\tplanned\n");
	CHECK(Steps()[0].label == "add Home Video");
}

TEST_CASE_FIXTURE(CliFixture, "cli add takes a score and the progress made already") {
	Seed();
	KnowTitle("Home Video", 900);
	KnowTitle("Ping Pong", 901);
	CHECK(Run({"add", "-q", "--progress=3", "--score=7", "Home Video"}) == 0);
	auto added = SavedTitle(MediaType::kAnime, "Home Video");
	CHECK(added.status == WatchStatus::kWatching);
	CHECK(added.progress == 3);
	CHECK(added.score == 7);
	CHECK(catalist::FormatDate(added.start_date) == "2026-10-20");
	CHECK(Run({"add", "-q", "--progress=2", "--status=on-hold", "Ping Pong"}) == 0);
	auto held = SavedTitle(MediaType::kAnime, "Ping Pong");
	CHECK(held.status == WatchStatus::kOnHold);
	CHECK(held.progress == 2);
	CHECK(Steps().size() == 2);

	CHECK(Run({"add", "--score=11", "x"}) == 2);
	CHECK(err.find("error: --score '11' is not 0 to 10\n") == 0);
	CHECK(Run({"add", "--progress=lots", "x"}) == 2);
	CHECK(err.find("error: --progress 'lots' is not a count\n") == 0);
}
