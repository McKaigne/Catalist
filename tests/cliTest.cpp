#include <doctest/doctest.h>

#include <algorithm>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#include "cli.hpp"
#include "malXml.hpp"
#include "support/testSupport.hpp"
#include "version.hpp"

namespace {

using catalist::mediaType;
using catalist::watchStatus;
using catalist::test::makeEntry;
using catalist::test::testToday;

const char* const kAnimeXml =
    "<myanimelist>"
    "<anime><series_animedb_id>52991</series_animedb_id>"
    "<series_title><![CDATA[Sousou no Frieren]]></series_title>"
    "<series_episodes>28</series_episodes>"
    "<my_watched_episodes>28</my_watched_episodes><my_score>10</my_score>"
    "<my_status>Completed</my_status></anime>"
    "<anime><series_animedb_id>44265</series_animedb_id>"
    "<series_title><![CDATA[Odd Taxi]]></series_title>"
    "<series_episodes>13</series_episodes>"
    "<my_watched_episodes>5</my_watched_episodes><my_score>8</my_score>"
    "<my_status>Watching</my_status></anime>"
    "</myanimelist>";

const char* const kMangaXml =
    "<myanimelist>"
    "<manga><manga_mangadb_id>2</manga_mangadb_id>"
    "<manga_title><![CDATA[Berserk]]></manga_title>"
    "<my_read_chapters>364</my_read_chapters><my_score>10</my_score>"
    "<my_status>Reading</my_status></manga>"
    "</myanimelist>";

// One `catalist ...` run with the data in data/ of a temporary directory,
// which is the working directory while the test runs, so file names in the
// output stay short. Each run gets a fresh catalogue, as each real
// invocation does; AniList is a fake that knows only what a test tells it.
struct cliFixture : catalist::test::tempDirFixture {
	cliFixture() : previousDir(std::filesystem::current_path()) {
		std::filesystem::current_path(tempDir());
	}
	~cliFixture() { std::filesystem::current_path(previousDir); }

	// `input` is what the person types when asked; `columns` 0 is a pipe.
	int run(const std::vector<std::string>& args, const std::string& input = "", int columns = 0) {
		catalist::catalogue db("data");
		std::istringstream inStream(input);
		std::ostringstream outStream;
		std::ostringstream errStream;
		catalist::cliContext ctx{db, inStream, outStream, errStream, testToday, web.poster()};
		ctx.interactive = not input.empty();
		ctx.columns = columns;
		int exitCode = catalist::runCli(args, ctx);
		out = outStream.str();
		err = errStream.str();
		return exitCode;
	}

	std::vector<catalist::entry> saved(mediaType type) {
		catalist::catalogue db("data");
		REQUIRE(db.loadAll().ok());
		return db.getEntries(type);
	}

	catalist::entry savedTitle(mediaType type, const std::string& title) {
		catalist::catalogue db("data");
		REQUIRE(db.loadAll().ok());
		return catalist::test::findTitle(db, type, title);
	}

	void put(const std::string& path, const std::string& content) {
		catalist::test::writeText(path, content);
	}

	// Four titles, two of them called Frieren, all checked and linked today
	// so no command asks AniList unless a test wants it to.
	void seed() {
		catalist::catalogue db("data");
		db.addEntry(makeEntry()
		                .title("Sousou no Frieren")
		                .malId(52991)
		                .anilistId(154587)
		                .status(watchStatus::kCompleted)
		                .score(10)
		                .progress(28)
		                .totalUnits(28)
		                .checkedAt("2026-10-20"));
		db.addEntry(makeEntry()
		                .title("Odd Taxi")
		                .malId(44265)
		                .anilistId(128547)
		                .status(watchStatus::kWatching)
		                .score(8)
		                .progress(5)
		                .totalUnits(13)
		                .released(13)
		                .checkedAt("2026-10-20"));
		db.addEntry(makeEntry()
		                .title("Berserk")
		                .type(mediaType::kManga)
		                .malId(2)
		                .anilistId(30002)
		                .status(watchStatus::kWatching)
		                .progress(364)
		                .checkedAt("2026-10-20"));
		db.addEntry(makeEntry()
		                .title("Sousou no Frieren")
		                .type(mediaType::kManga)
		                .malId(126287)
		                .anilistId(118586)
		                .status(watchStatus::kPlanToWatch)
		                .checkedAt("2026-10-20"));
		db.setLinkedOn(testToday());
		REQUIRE(db.saveAll());
		REQUIRE(db.saveSettings());
	}

	catalist::test::fakeWeb web;
	std::filesystem::path previousDir;
	std::string out;
	std::string err;
};

bool has(const std::string& text, const std::string& part) {
	return text.find(part) != std::string::npos;
}

}  // namespace

TEST_CASE_FIXTURE(cliFixture, "cli --version prints the version") {
	for (const char* flag : {"--version", "-V", "-v", "version"}) {
		CHECK(run({flag}) == 0);
		CHECK(out == std::string("Catalist v") + catalist::kVersionString + "\n");
		CHECK(err.empty());
	}
}

TEST_CASE_FIXTURE(cliFixture, "cli help lists the commands, and each command has its own") {
	for (const char* flag : {"--help", "-h", "help"}) {
		CHECK(run({flag}) == 0);
		CHECK(has(out, "  ls        List your titles"));
		CHECK(has(out, "  add       Add a title found on AniList"));
		CHECK(has(out, "catalist next frieren"));
		CHECK(has(out, "kept in\ndata."));
		CHECK(err.empty());
	}
	CHECK(run({"help", "set"}) == 0);
	CHECK(has(out, "Usage: catalist set [anime|manga] <TITLE|ID> <options>"));
	CHECK(has(out, "-p, --progress N"));
	CHECK(has(out, "Also: edit"));
	std::string fromHelp = out;
	CHECK(run({"edit", "--help"}) == 0);
	CHECK(out == fromHelp);
	CHECK(run({"set", "-h"}) == 0);
	CHECK(out == fromHelp);
	CHECK(run({"help", "nope"}) == 1);
}

TEST_CASE_FIXTURE(cliFixture, "cli help and version work with a broken catalogue") {
	put("data/anime.csv", "this,is,not,a,catalogue\n");
	CHECK(run({"--version"}) == 0);
	CHECK(run({"--help"}) == 0);
	CHECK(run({"set", "--help"}) == 0);
}

TEST_CASE_FIXTURE(cliFixture, "cli points out a mistyped command or option, with exit code 2") {
	CHECK(run({"lst"}) == 2);
	CHECK(err ==
	      "error: unknown command 'lst' (did you mean 'ls'?)\n"
	      "Run 'catalist help' to see the commands.\n");
	CHECK(out.empty());
	CHECK(run({"sync"}) == 2);
	CHECK(err.find("error: unknown command 'sync'\n") == 0);
	CHECK(run({"--frobnicate"}) == 2);
	CHECK(err.find("error: unknown option '--frobnicate'") == 0);

	CHECK(run({"set", "frieren", "--progres", "3"}) == 2);
	CHECK(err ==
	      "error: unknown option '--progres' (did you mean '--progress'?)\n"
	      "Usage: catalist set [anime|manga] <TITLE|ID> <options>\n"
	      "Run 'catalist help set' for more.\n");
	CHECK(run({"set", "frieren", "--rating"}) == 2);
	CHECK(err.find("error: --rating needs a value (N)") == 0);
	CHECK(run({"ls", "--reverse=yes"}) == 2);
	CHECK(err.find("error: --reverse takes no value") == 0);
	CHECK(run({"import"}) == 2);
	CHECK(err.find("error: give at least one file") == 0);
	CHECK(run({"export", "books"}) == 2);
	CHECK(run({"export", "anime", "manga"}) == 2);
	CHECK(out.empty());
}

TEST_CASE_FIXTURE(cliFixture, "cli ls prints tab-separated lines into a pipe") {
	seed();
	CHECK(run({"ls"}) == 0);
	// New releases first, then the latest change.
	CHECK(out.find("anime\t128547\tOdd Taxi\tWatching\t8\t5\t13\t8\n") == 0);
	CHECK(has(out, "manga\t30002\tBerserk\tReading\t0\t364\t0\t0\n"));
	CHECK(std::count(out.begin(), out.end(), '\n') == 4);
	CHECK(err.empty());

	CHECK(run({"ls", "manga"}) == 0);
	CHECK(std::count(out.begin(), out.end(), '\n') == 2);
	CHECK(run({"ls", "FRIEREN"}) == 0);
	CHECK(std::count(out.begin(), out.end(), '\n') == 2);
	CHECK(run({"ls", "-s", "reading", "anime"}) == 0);
	CHECK(out == "anime\t128547\tOdd Taxi\tWatching\t8\t5\t13\t8\n");
	CHECK(run({"ls", "--sort", "title", "-n", "1"}) == 0);
	CHECK(out.find("manga\t30002\tBerserk") == 0);
	CHECK(std::count(out.begin(), out.end(), '\n') == 1);
	CHECK(run({"ls", "--sort", "title", "--reverse", "--limit=1"}) == 0);
	CHECK(has(out, "Sousou no Frieren"));

	CHECK(run({"ls", "--status", "bored"}) == 2);
	CHECK(err.find("error: unknown status 'bored'") == 0);
	CHECK(run({"ls", "--sort", "colour"}) == 2);
}

TEST_CASE_FIXTURE(cliFixture, "cli ls draws a table at a terminal") {
	seed();
	CHECK(run({"ls", "anime"}, "", 100) == 0);
	CHECK(has(out, "ID  TITLE"));
	CHECK(has(out, "STATUS"));
	CHECK(has(out, "128547  Odd Taxi"));
	CHECK(has(out, "5/13"));
	CHECK(has(out, "+8"));
	CHECK(has(out, "2 titles\n"));
	// One type: no type column.
	CHECK_FALSE(has(out, "TYPE"));
	CHECK(run({"ls", "-n", "1"}, "", 100) == 0);
	CHECK(has(out, "TYPE"));
	CHECK(has(out, "1 of 4 titles"));
	CHECK(run({"ls", "nothing like it"}, "", 100) == 0);
	CHECK(out == "Nothing to show.\n");
}

TEST_CASE_FIXTURE(cliFixture, "cli show finds a title by words or by the provider's id") {
	seed();
	CHECK(run({"show", "taxi"}) == 0);
	CHECK(out.find("Odd Taxi\n") == 0);
	CHECK(has(out, "Anime, AniList #128547"));
	CHECK(has(out, "https://anilist.co/anime/128547"));
	CHECK(has(out, "5/13 episodes  (+8 new)"));
	CHECK(has(out, "8/10"));
	CHECK(run({"show", "30002"}) == 0);
	CHECK(out.find("Berserk\n") == 0);
	CHECK(has(out, "Volumes"));
	// A title that is exactly the words wins over longer ones that hold them.
	CHECK(run({"show", "manga", "sousou", "no", "frieren"}) == 0);
	CHECK(has(out, "Manga, AniList #118586"));

	CHECK(run({"show", "naruto"}) == 1);
	CHECK(err ==
	      "error: nothing in your lists matches 'naruto'\n"
	      "See what is there with: catalist ls\n");
	CHECK(run({"show"}) == 2);
}

TEST_CASE_FIXTURE(cliFixture, "cli asks which title was meant, or lists them into a pipe") {
	seed();
	CHECK(run({"show", "frieren"}) == 1);
	CHECK(err ==
	      "error: 'frieren' matches 2 titles:\n"
	      "  anime   154587  Sousou no Frieren\n"
	      "  manga   118586  Sousou no Frieren\n"
	      "Name one by its AniList id, or with more of its title.\n");
	CHECK(out.empty());

	CHECK(run({"show", "frieren"}, "2\n") == 0);
	CHECK(has(err, "   2  manga   118586  Sousou no Frieren"));
	CHECK(has(err, "Which one? [1-2, Enter to cancel]: "));
	CHECK(has(out, "Manga, AniList #118586"));
	CHECK(run({"show", "frieren"}, "\n") == 1);
	CHECK(has(err, "Canceled."));
}

TEST_CASE_FIXTURE(cliFixture, "cli set changes a title and says what changed") {
	seed();
	CHECK(run({"set", "taxi", "-p", "13", "--rating=9"}) == 0);
	CHECK(out ==
	      "Odd Taxi\n"
	      "  status    Watching → Completed\n"
	      "  rating    8 → 9\n"
	      "  progress  5/13 → 13/13\n");
	auto taxi = savedTitle(mediaType::kAnime, "Odd Taxi");
	CHECK(taxi.status == watchStatus::kCompleted);
	CHECK(catalist::formatDate(taxi.finishDate) == "2026-10-20");

	CHECK(run({"edit", "taxi", "--progress", "-3"}) == 0);
	CHECK(has(out, "progress  13/13 → 10/13"));
	CHECK(run({"set", "manga", "berserk", "-s", "on-hold", "--volumes", "40"}) == 0);
	CHECK(has(out, "status    Reading → On-Hold"));
	CHECK(has(out, "volumes   0 → 40"));
	CHECK(run({"set", "taxi", "-r", "9"}) == 0);
	CHECK(has(out, "nothing changed"));
}

TEST_CASE_FIXTURE(cliFixture, "cli set checks its values before touching anything") {
	seed();
	CHECK(run({"set", "taxi"}) == 2);
	CHECK(err.find("error: nothing to change") == 0);
	CHECK(run({"set", "taxi", "-r", "11"}) == 2);
	CHECK(err.find("error: --rating '11' is not 0 to 10") == 0);
	CHECK(run({"set", "taxi", "-s", "bored"}) == 2);
	CHECK(run({"set", "taxi", "-p", "lots"}) == 2);
	CHECK(run({"set", "taxi", "-p", "1234567"}) == 2);
	CHECK(savedTitle(mediaType::kAnime, "Odd Taxi").progress == 5);
}

TEST_CASE_FIXTURE(cliFixture, "cli refreshes a title from AniList before changing it") {
	seed();
	// Berserk has ended since it was last checked (on a fresh day).
	put("data/settings", "provider = anilist\nlinked = 2026-10-20\n");
	{
		catalist::catalogue db("data");
		REQUIRE(db.loadAll().ok());
		auto berserk = catalist::test::findTitle(db, mediaType::kManga, "Berserk");
		berserk.checkedAt.reset();
		REQUIRE(db.updateEntry(berserk));
		REQUIRE(db.saveAll());
	}
	web.respond(R"({"id":30002,"type":"MANGA"})",
	            R"({"data":{"Media":{"id":30002,"idMal":2,"status":"FINISHED","chapters":380}}})");
	CHECK(run({"next", "berserk"}) == 0);
	CHECK(out == "Berserk\n  progress  364/? → 365/380\n");
	auto berserk = savedTitle(mediaType::kManga, "Berserk");
	CHECK(berserk.totalUnits == 380);
	CHECK(berserk.released == 380);
	CHECK(catalist::formatDate(berserk.checkedAt) == "2026-10-20");

	// Checked today: not asked again.
	size_t asked = web.posted.size();
	CHECK(run({"next", "berserk", "--by", "15"}) == 0);
	CHECK(out == "Berserk\n  status    Reading → Completed\n  progress  365/380 → 380/380\n");
	CHECK(web.posted.size() == asked);
}

TEST_CASE_FIXTURE(cliFixture, "cli next counts one more, offline too") {
	seed();
	CHECK(run({"next", "128547"}) == 0);
	CHECK(out == "Odd Taxi\n  progress  5/13 → 6/13\n");
	CHECK(run({"next", "taxi", "-n", "0"}) == 2);
	CHECK(run({"next", "taxi", "-n", "x"}) == 2);
	CHECK(savedTitle(mediaType::kAnime, "Odd Taxi").progress == 6);
}

TEST_CASE_FIXTURE(cliFixture, "cli rm asks first, and wants --yes in a pipe") {
	seed();
	CHECK(run({"rm", "taxi"}) == 1);
	CHECK(err == "error: pass --yes to remove Odd Taxi without being asked\n");
	CHECK(run({"rm", "taxi"}, "n\n") == 1);
	CHECK(has(err, "Remove Odd Taxi from your anime list? [y/N]: "));
	CHECK(saved(mediaType::kAnime).size() == 2);
	CHECK(run({"remove", "taxi"}, "y\n") == 0);
	CHECK(out == "Removed Odd Taxi.\n");
	CHECK(run({"delete", "anime", "frieren", "--yes"}) == 0);
	CHECK(saved(mediaType::kAnime).empty());
	CHECK(saved(mediaType::kManga).size() == 2);
}

TEST_CASE_FIXTURE(cliFixture, "cli add finds the title on AniList and adds it") {
	seed();
	web.respond(R"("search":"dandadan","type":"ANIME")", R"({"data":{"Page":{"media":[
	  {"id":171018,"idMal":57334,"title":{"romaji":"Dandadan"},"episodes":12,
	   "startDate":{"year":2024}}]}}})");
	web.respond(R"({"id":171018,"type":"ANIME")", R"({"data":{"Media":{"id":171018,
	  "idMal":57334,"title":{"romaji":"Dandadan"},"episodes":12,"status":"FINISHED"}}})");
	CHECK(run({"add", "anime", "dandadan", "-s", "watching", "-p", "3"}) == 0);
	CHECK(out == "Added Dandadan to your anime list as Watching.\n");
	auto added = savedTitle(mediaType::kAnime, "Dandadan");
	CHECK(added.anilistId == 171018);
	CHECK(added.malId == 57334);
	CHECK(added.totalUnits == 12);
	CHECK(added.released == 12);
	CHECK(added.progress == 3);
	CHECK(catalist::formatDate(added.startDate) == "2026-10-20");

	// Once is enough.
	CHECK(run({"add", "anime", "dandadan"}) == 1);
	CHECK(err ==
	      "error: Dandadan is already in your anime list (Watching)\n"
	      "Change it with: catalist set 171018 ...\n");
	// By id too.
	CHECK(run({"add", "anime", "171018"}) == 1);
	CHECK(has(err, "already in your anime list"));
}

TEST_CASE_FIXTURE(cliFixture, "cli add with several matches asks, or lists them into a pipe") {
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

	CHECK(run({"add", "anime", "fullmetal"}) == 1);
	CHECK(err ==
	      "error: 'fullmetal' matches 2 titles on AniList:\n"
	      "     5114  2009  Fullmetal Alchemist: Brotherhood  (64 episodes)\n"
	      "      121  2003  Fullmetal Alchemist  (51 episodes)\n"
	      "Add one by its AniList id, e.g. catalist add anime 5114, or pass --yes for "
	      "the first.\n");
	CHECK(saved(mediaType::kAnime).size() == 2);

	CHECK(run({"add", "anime", "fullmetal"}, "2\n") == 0);
	CHECK(out == "Added Fullmetal Alchemist to your anime list as Plan to Watch.\n");
	CHECK(run({"add", "anime", "fullmetal", "--yes"}) == 0);
	CHECK(has(out, "Added Fullmetal Alchemist: Brotherhood"));
	CHECK(saved(mediaType::kAnime).size() == 4);
}

TEST_CASE_FIXTURE(cliFixture, "cli add says what went wrong") {
	seed();
	CHECK(run({"add", "dandadan"}) == 2);
	CHECK(has(err, "say which list"));
	CHECK(run({"add", "anime"}) == 2);
	CHECK(run({"add", "anime", "x", "-r", "12"}) == 2);
	// The fake web answers nothing: offline.
	CHECK(run({"add", "anime", "dandadan"}) == 1);
	CHECK(err == "error: could not reach AniList; check the connection and try again\n");
	web.respond(R"("search":"zzz")", R"({"data":{"Page":{"media":[]}}})");
	CHECK(run({"add", "manga", "zzz"}) == 1);
	CHECK(err == "error: AniList has no manga matching 'zzz'\n");
	web.respond(R"({"id":999,"type":"ANIME")", R"({"data":{"Media":null}})");
	CHECK(run({"add", "anime", "999"}) == 1);
	CHECK(err == "error: AniList knows no anime with AniList id 999\n");
}

TEST_CASE_FIXTURE(cliFixture, "cli search shows the matches and which are tracked") {
	seed();
	web.respond(R"("search":"frieren","type":"ANIME")", R"({"data":{"Page":{"media":[
	  {"id":154587,"idMal":52991,"title":{"romaji":"Sousou no Frieren"},"episodes":28,
	   "startDate":{"year":2023}},
	  {"id":182255,"idMal":59978,"title":{"romaji":"Sousou no Frieren 2nd Season"},
	   "startDate":{"year":2026}}]}}})");
	CHECK(run({"search", "anime", "frieren"}) == 0);
	CHECK(out ==
	      "154587\t2023\tSousou no Frieren\t28\ttracked\n"
	      "182255\t2026\tSousou no Frieren 2nd Season\t0\t\n");
	CHECK(run({"search", "anime", "frieren"}, "", 100) == 0);
	CHECK(has(out, " 154587  2023  Sousou no Frieren  (28 episodes)  in your list\n"));
	CHECK(has(out, "(running)\n"));
	CHECK(run({"search", "frieren"}) == 2);
}

TEST_CASE_FIXTURE(cliFixture, "cli provider decides which ids are shown and accepted") {
	seed();
	CHECK(run({"provider"}) == 0);
	CHECK(out == "Your provider is AniList.\n");
	CHECK(run({"provider", "mal"}) == 0);
	CHECK(has(out, "now MyAnimeList"));
	CHECK(has(catalist::test::readText("data/settings"), "provider = myanimelist\n"));
	CHECK(run({"show", "44265"}) == 0);
	CHECK(has(out, "Anime, MyAnimeList #44265"));
	CHECK(has(out, "https://myanimelist.net/anime/44265"));
	CHECK(run({"ls", "anime", "taxi"}) == 0);
	CHECK(out.find("anime\t44265\t") == 0);
	CHECK(run({"show", "128547"}) == 1);

	CHECK(run({"provider", "kitsu"}) == 2);
	CHECK(run({"provider", "anilist"}) == 0);
	CHECK(run({"show", "128547"}) == 0);
}

TEST_CASE_FIXTURE(cliFixture, "cli looks up missing ids once a day") {
	put("lists/anime.xml", kAnimeXml);
	REQUIRE(run({"import", "lists/anime.xml"}) == 0);
	// AniList knows Frieren but not (in this test) Odd Taxi.
	web.respond(R"("ids":[52991,44265])",
	            R"({"data":{"Page":{"media":[{"id":154587,"idMal":52991}]}}})");
	CHECK(run({"ls"}) == 0);
	CHECK(has(out, "anime\t154587\tSousou no Frieren"));
	CHECK(has(out, "anime\t-\tOdd Taxi"));
	CHECK(savedTitle(mediaType::kAnime, "Sousou no Frieren").anilistId == 154587);
	CHECK(web.posted.size() == 1);
	CHECK(has(catalist::test::readText("data/settings"), "linked = 2026-10-20\n"));
	// Odd Taxi still lacks its id, but it was asked about today.
	CHECK(run({"ls"}) == 0);
	CHECK(web.posted.size() == 1);
	// A new import asks again on the next command.
	REQUIRE(run({"import", "lists/anime.xml"}) == 0);
	CHECK(run({"ls"}) == 0);
	CHECK(web.posted.size() == 2);
}

TEST_CASE_FIXTURE(cliFixture, "cli import adds the titles and saves them") {
	put("lists/anime.xml", kAnimeXml);
	put("lists/manga.xml", kMangaXml);

	CHECK(run({"import", "lists/anime.xml", "lists/manga.xml"}) == 0);
	CHECK(out ==
	      "lists/anime.xml: 2 titles, 2 added, 0 already in the catalogue\n"
	      "lists/manga.xml: 1 titles, 1 added, 0 already in the catalogue\n");
	CHECK(err.empty());

	auto anime = saved(mediaType::kAnime);
	REQUIRE(anime.size() == 2);
	CHECK(anime[0].title == "Sousou no Frieren");
	CHECK(anime[0].malId == 52991);
	CHECK(anime[0].score == 10);
	CHECK(anime[0].progress == 28);
	CHECK(anime[0].status == catalist::watchStatus::kCompleted);
	CHECK(anime[1].title == "Odd Taxi");
	auto manga = saved(mediaType::kManga);
	REQUIRE(manga.size() == 1);
	CHECK(manga[0].title == "Berserk");
	CHECK(manga[0].progress == 364);
}

TEST_CASE_FIXTURE(cliFixture, "cli importing again keeps what the user changed") {
	put("lists/anime.xml", kAnimeXml);
	REQUIRE(run({"import", "lists/anime.xml"}) == 0);

	{
		catalist::catalogue db("data");
		REQUIRE(db.loadAll().ok());
		auto taxi = db.getEntries(mediaType::kAnime)[1];
		REQUIRE(taxi.title == "Odd Taxi");
		taxi.score = 9;
		taxi.progress = 13;
		taxi.startDate = catalist::test::requireDate("2026-01-02");
		REQUIRE(db.updateEntry(taxi));
		REQUIRE(db.saveAll());
	}

	CHECK(run({"import", "lists/anime.xml"}) == 0);
	CHECK(out == "lists/anime.xml: 2 titles, 0 added, 2 already in the catalogue\n");
	auto anime = saved(mediaType::kAnime);
	REQUIRE(anime.size() == 2);
	CHECK(anime[1].score == 9);
	CHECK(anime[1].progress == 13);
	CHECK(catalist::formatDate(anime[1].startDate) == "2026-01-02");
}

TEST_CASE_FIXTURE(cliFixture, "cli import changes nothing when any file is bad") {
	put("lists/anime.xml", kAnimeXml);
	put("lists/empty.xml", "<myanimelist></myanimelist>");

	CHECK(run({"import", "lists/anime.xml", "lists/missing.xml"}) == 1);
	CHECK(err == "error: cannot read lists/missing.xml\n");
	CHECK(out.empty());
	CHECK(catalist::test::readText("data/anime.csv").empty());

	put("lists/anime.xml.gz", std::string("\x1f\x8b\x08\x00", 4) + "gz");
	CHECK(run({"import", "lists/anime.xml.gz"}) == 1);
	CHECK(err ==
	      "error: lists/anime.xml.gz is compressed; unpack it first "
	      "(gunzip lists/anime.xml.gz)\n");

	CHECK(run({"import", "lists/anime.xml", "lists/empty.xml"}) == 1);
	CHECK(err ==
	      "error: no anime or manga found in lists/empty.xml (is it a "
	      "MyAnimeList XML export?)\n");
	CHECK(catalist::test::readText("data/anime.csv").empty());
}

TEST_CASE_FIXTURE(cliFixture, "cli import exits 1 when the catalogue cannot be saved") {
	put("lists/anime.xml", kAnimeXml);
	catalist::test::makeUnwritable("data/anime.csv");
	CHECK(run({"import", "lists/anime.xml"}) == 1);
	CHECK(err == "error: could not save the catalogue to data\n");
}

TEST_CASE_FIXTURE(cliFixture, "cli refuses to work on a catalogue it cannot read") {
	put("data/anime.csv", "this,is,not,a,catalogue\n");
	put("lists/anime.xml", kAnimeXml);
	CHECK(run({"import", "lists/anime.xml"}) == 1);
	CHECK(err.find("error: data/anime.csv: ") == 0);
	CHECK(run({"export", "anime"}) == 1);
	CHECK(out.empty());
	CHECK(catalist::test::readText("data/anime.csv") == "this,is,not,a,catalogue\n");
}

TEST_CASE_FIXTURE(cliFixture, "cli export prints what import read") {
	put("lists/anime.xml", kAnimeXml);
	put("lists/manga.xml", kMangaXml);
	REQUIRE(run({"import", "lists/anime.xml", "lists/manga.xml"}) == 0);

	CHECK(run({"export", "anime"}) == 0);
	CHECK(err.empty());
	auto anime = catalist::malXml::parse(out);
	REQUIRE(anime.size() == 2);
	CHECK(anime[0].title == "Sousou no Frieren");
	CHECK(anime[0].malId == 52991);
	CHECK(anime[0].progress == 28);
	CHECK(anime[1].title == "Odd Taxi");
	CHECK(anime[1].score == 8);

	CHECK(run({"export", "manga"}) == 0);
	auto manga = catalist::malXml::parse(out);
	REQUIRE(manga.size() == 1);
	CHECK(manga[0].type == mediaType::kManga);
	CHECK(manga[0].title == "Berserk");
}

TEST_CASE_FIXTURE(cliFixture, "cli export notes the titles it had to leave out") {
	{
		catalist::catalogue db("data");
		db.addEntry(catalist::test::makeEntry().title("Home video"));
		db.addEntry(catalist::test::makeEntry().title("Another one"));
		REQUIRE(db.saveAll());
	}
	CHECK(run({"export", "anime"}) == 0);
	CHECK(err == "note: 2 titles without a MyAnimeList id were left out.\n");
	CHECK(catalist::malXml::parse(out).empty());
	CHECK(out.find("<myanimelist>") != std::string::npos);
}

TEST_CASE_FIXTURE(cliFixture, "cli export of an empty catalogue is an empty list") {
	CHECK(run({"export", "manga"}) == 0);
	CHECK(err.empty());
	CHECK(out.find("<user_export_type>2</user_export_type>") != std::string::npos);
	CHECK(catalist::malXml::parse(out).empty());
}
