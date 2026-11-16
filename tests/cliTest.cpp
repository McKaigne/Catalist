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
		if (outClosed)
			outStream.setstate(std::ios::badbit);
		catalist::cliContext ctx{db, inStream, outStream, errStream, testToday, web.poster()};
		ctx.interactive = not input.empty();
		ctx.columns = columns;
		ctx.color = color;
		ctx.program = program;
		ctx.nowStamp = [] { return std::string(kStamp); };
		ctx.newTxLabel = [] { return std::string("1792531800-4242"); };
		ctx.txLabel = group;
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

	// Four titles, two of them called Frieren (anime a:1 and manga m:2; Odd
	// Taxi is a:2 and Berserk m:1), all checked and linked today so no
	// command asks AniList unless a test wants it to.
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
		                .startDate("2026-10-01")
		                .checkedAt("2026-10-20"));
		db.addEntry(makeEntry()
		                .title("Berserk")
		                .type(mediaType::kManga)
		                .malId(2)
		                .anilistId(30002)
		                .status(watchStatus::kWatching)
		                .progress(364)
		                .startDate("2019-05-01")
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

	// The steps of the undo history, newest first.
	std::vector<catalist::txRecord> steps() {
		catalist::catalogue db("data");
		REQUIRE(db.loadAll().ok());
		return db.history(0).value();
	}

	// A change no command made, as a web check in the tracker makes one.
	void changeBehindTheHistory(mediaType type, const std::string& title, int progress) {
		catalist::catalogue db("data");
		REQUIRE(db.loadAll().ok());
		auto changed = catalist::test::findTitle(db, type, title);
		changed.progress = progress;
		REQUIRE(db.updateEntry(changed));
		REQUIRE(db.saveAll());
	}

	// When every change of a test is made.
	static constexpr const char* kStamp = "2026-10-20 21:30:00";

	catalist::test::fakeWeb web;
	std::filesystem::path previousDir;
	// How the next runs are set up: the name the program goes by, colours,
	// a standard output whose reader has gone, and CATALIST_TX.
	std::string program = "catalist";
	bool color = false;
	bool outClosed = false;
	std::string group;
	std::string out;
	std::string err;
};

bool has(const std::string& text, const std::string& part) {
	return text.find(part) != std::string::npos;
}

}  // namespace

TEST_CASE_FIXTURE(cliFixture, "cli --version prints the version") {
	for (const char* flag : {"--version", "-V", "-v"}) {
		CHECK(run({flag}) == 0);
		CHECK(out == std::string("Catalist v") + catalist::kVersionString + "\n");
		CHECK(err.empty());
	}
}

TEST_CASE_FIXTURE(cliFixture, "cli help lists the commands, and each command has its own") {
	for (const char* flag : {"--help", "-h", "help"}) {
		CHECK(run({flag}) == 0);
		CHECK(has(out, "Usage: catalist [COMMAND] [ARG...]\n"));
		CHECK(has(out, "  ls          List your titles\n"));
		CHECK(has(out, "  +           Count one more episode or chapter\n"));
		CHECK(has(out, "  undo        Take back what the last command changed\n"));
		CHECK(has(out, "  help        Show help for a command\n"));
		CHECK(has(out, "  add         Add a title found on AniList\n"));
		CHECK(has(out, "  catalist show a:12\n"));
		CHECK(has(out, "kept in\ndata.\n"));
		CHECK(err.empty());
	}
	CHECK(run({"help", "ls"}) == 0);
	CHECK(out.find("List your titles.\n\n"
	               "Usage: catalist ls [-amrq] [-s STATUS] [--sort=KEY] [--json] [--header] "
	               "[QUERY]\n") == 0);
	CHECK(has(out, "ref type title status score progress total volumes rewatched\n"));
	CHECK(has(out, "\nOptions:\n  -a                list anime only\n"));
	CHECK(has(out, "  -s STATUS         only watching, completed, on-hold, dropped or planned\n"));
	CHECK(has(out, "      --sort=KEY    new, updated, score, progress, date, status or title\n"));
	CHECK(out.rfind("  -h, --help        show this help\n") + 35 == out.size());
	CHECK(err.empty());
	std::string fromHelp = out;
	for (const char* flag : {"-h", "--help"}) {
		CHECK(run({"ls", flag}) == 0);
		CHECK(out == fromHelp);
	}
	// Asked for among other options, help is all that happens.
	CHECK(run({"ls", "-qh", "--sort=title"}) == 0);
	CHECK(out == fromHelp);
	CHECK(run({"help", "show"}) == 0);
	CHECK(has(out, "Usage: catalist show [-am] [--all] [--json] [--header] REF...\n"));
	CHECK(run({"help", "+"}) == 0);
	CHECK(has(out, "Usage: catalist + [-amq] [--all] [-n N] REF...\nAlso:  catalist inc\n"));
	CHECK(run({"-", "--help"}) == 0);
	CHECK(has(out, "Usage: catalist - [-amq] [--all] [-n N] REF...\nAlso:  catalist dec\n"));
	CHECK(run({"help", "set"}) == 0);
	CHECK(has(out, "Usage: catalist set [-amq] [--all] REF FIELD=VALUE...\n"));
	CHECK(run({"help", "done"}) == 0);
	CHECK(has(out, "Usage: catalist done [-amq] [--all] [--score=N] REF...\n"));
	CHECK(run({"help", "hold"}) == 0);
	CHECK(has(out, "Usage: catalist hold [-amq] [--all] REF...\n"));
	CHECK(run({"help", "undo"}) == 0);
	CHECK(has(out, "Usage: catalist undo [-fq] [-n N]\n"));
	CHECK(run({"help", "tx"}) == 0);
	CHECK(has(out, "export CATALIST_TX=$(catalist tx)\n"));
	// A command that still has its old form.
	CHECK(run({"help", "rm"}) == 0);
	CHECK(has(out, "Usage: catalist rm [anime|manga] <TITLE|ID>"));

	CHECK(run({"help", "frobnicate"}) == 2);
	CHECK(err ==
	      "error: no command 'frobnicate'\n"
	      "Usage: catalist help [COMMAND]\n"
	      "Run 'catalist help help' for more.\n");
	CHECK(out.empty());
	CHECK(run({"help", "shwo"}) == 2);
	CHECK(err.find("error: no command 'shwo' (did you mean 'show'?)\n") == 0);
	CHECK(run({"help", "ls", "show"}) == 2);
}

TEST_CASE_FIXTURE(cliFixture, "cli names itself as it was run") {
	program = "cl";
	CHECK(run({"help"}) == 0);
	CHECK(has(out, "Usage: cl [COMMAND] [ARG...]\n"));
	CHECK(has(out, "  cl show a:12\n"));
	CHECK(has(out, "Run 'cl help COMMAND'"));
	CHECK_FALSE(has(out, "catalist "));
	CHECK(run({"help", "ls"}) == 0);
	CHECK(has(out, "Usage: cl ls [-amrq]"));
	CHECK(run({"lst"}) == 2);
	CHECK(err ==
	      "error: unknown command 'lst' (did you mean 'ls'?)\n"
	      "Usage: cl [COMMAND] [ARG...]\n"
	      "Run 'cl help' to see the commands.\n");
	CHECK(run({"ls", "-x"}) == 2);
	CHECK(err ==
	      "error: unknown option '-x'\n"
	      "Usage: cl ls [-amrq] [-s STATUS] [--sort=KEY] [--json] [--header] [QUERY]\n"
	      "Run 'cl help ls' for more.\n");
	CHECK(run({"show", "naruto"}) == 1);
	CHECK(has(err, "See what is there with: cl ls\n"));
}

TEST_CASE_FIXTURE(cliFixture, "cli says once that it imported the lists of an earlier version") {
	put("data/anime.csv", catalist::test::readText(catalist::test::fixturePath("csv/crlf.csv")));
	CHECK(run({"ls"}) == 0);
	CHECK(has(out, "\tOdd Taxi\t"));
	CHECK(err ==
	      "note: imported the lists in data into data/catalist.db; the old files are left as "
	      "they were.\n");
	CHECK(run({"ls"}) == 0);
	CHECK(has(out, "\tOdd Taxi\t"));
	CHECK(err.empty());
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
	      "Usage: catalist [COMMAND] [ARG...]\n"
	      "Run 'catalist help' to see the commands.\n");
	CHECK(out.empty());
	CHECK(run({"frobnicate"}) == 2);
	CHECK(err.find("error: unknown command 'frobnicate'\n") == 0);
	CHECK(run({"--frobnicate"}) == 2);
	CHECK(err.find("error: unknown option '--frobnicate'\nUsage: catalist [COMMAND]") == 0);
	// The names the earlier command line had are gone.
	for (const char* gone : {"list", "info", "version", "next", "edit"})
		CHECK(run({gone}) == 2);
	CHECK(run({"udno"}) == 2);
	CHECK(err.find("error: unknown command 'udno' (did you mean 'undo'?)\n") == 0);

	CHECK(run({"ls", "--jsno"}) == 2);
	CHECK(err ==
	      "error: unknown option '--jsno' (did you mean '--json'?)\n"
	      "Usage: catalist ls [-amrq] [-s STATUS] [--sort=KEY] [--json] [--header] [QUERY]\n"
	      "Run 'catalist help ls' for more.\n");
	CHECK(run({"ls", "-ax"}) == 2);
	CHECK(err.find("error: unknown option '-x'\n") == 0);
	CHECK(run({"ls", "--sort"}) == 2);
	CHECK(err.find("error: option '--sort' needs a value (KEY)\n") == 0);
	CHECK(run({"ls", "-s"}) == 2);
	CHECK(err.find("error: option '-s' needs a value (STATUS)\n") == 0);
	CHECK(run({"ls", "--json=yes"}) == 2);
	CHECK(err.find("error: option '--json' takes no value\n") == 0);
	CHECK(run({"show", "--al", "taxi"}) == 2);
	CHECK(err.find("error: unknown option '--al' (did you mean '--all'?)\n") == 0);
	CHECK(out.empty());

	// The commands that still have their old form.
	CHECK(run({"add", "anime", "frieren", "--progres", "3"}) == 2);
	CHECK(err.find("error: unknown option '--progres' (did you mean '--progress'?)\n"
	               "Usage: catalist add ") == 0);
	CHECK(has(err, "Run 'catalist help add' for more.\n"));
	CHECK(run({"add", "anime", "frieren", "--rating"}) == 2);
	CHECK(err.find("error: --rating needs a value (N)") == 0);
	CHECK(run({"import"}) == 2);
	CHECK(err.find("error: give at least one file") == 0);
	CHECK(run({"export", "books"}) == 2);
	CHECK(run({"export", "anime", "manga"}) == 2);
	CHECK(out.empty());
}

TEST_CASE_FIXTURE(cliFixture, "cli ls prints tab-separated lines into a pipe") {
	seed();
	CHECK(run({"ls"}) == 0);
	// In title order; nothing but the titles, and every field holds something.
	CHECK(out ==
	      "m:1\tmanga\tBerserk\twatching\t0\t364\t0\t0\t0\t2019-05-01\t-\t-\t0\n"
	      "a:2\tanime\tOdd Taxi\twatching\t8\t5\t13\t0\t0\t2026-10-01\t-\t-\t8\n"
	      "a:1\tanime\tSousou no Frieren\tcompleted\t10\t28\t28\t0\t0\t-\t-\t-\t0\n"
	      "m:2\tmanga\tSousou no Frieren\tplanned\t0\t0\t0\t0\t0\t-\t-\t-\t0\n");
	CHECK(err.empty());

	CHECK(run({"ls", "--header", "taxi"}) == 0);
	CHECK(out ==
	      "ref\ttype\ttitle\tstatus\tscore\tprogress\ttotal\tvolumes\trewatched\tstarted\t"
	      "finished\tupdated\tnew\n"
	      "a:2\tanime\tOdd Taxi\twatching\t8\t5\t13\t0\t0\t2026-10-01\t-\t-\t8\n");
	// Nothing listed: nothing printed, and that is not a failure.
	CHECK(run({"ls", "nothing like it"}) == 0);
	CHECK(out.empty());
	CHECK(err.empty());
}

TEST_CASE_FIXTURE(cliFixture, "cli ls keeps to a list, a status or a part of the title") {
	seed();
	CHECK(run({"ls", "-q"}) == 0);
	CHECK(out == "m:1\na:2\na:1\nm:2\n");
	CHECK(run({"ls", "-qm"}) == 0);
	CHECK(out == "m:1\nm:2\n");
	CHECK(run({"ls", "-q", "-a"}) == 0);
	CHECK(out == "a:2\na:1\n");
	CHECK(run({"ls", "-qam"}) == 0);
	CHECK(out == "m:1\na:2\na:1\nm:2\n");
	CHECK(run({"ls", "-q", "FRIEREN"}) == 0);
	CHECK(out == "a:1\nm:2\n");
	CHECK(run({"ls", "-q", "no fri"}) == 0);
	CHECK(out == "a:1\nm:2\n");
	CHECK(run({"ls", "-q", "-s", "watching"}) == 0);
	CHECK(out == "m:1\na:2\n");
	CHECK(run({"ls", "-qas", "reading"}) == 0);
	CHECK(out == "a:2\n");
	CHECK(run({"ls", "-q", "-splanned", "sousou"}) == 0);
	CHECK(out == "m:2\n");
	CHECK(run({"ls", "-q", "-s", "on-hold"}) == 0);
	CHECK(out.empty());
	// After --, a title may start with a dash.
	CHECK(run({"ls", "-q", "--", "-s"}) == 0);
	CHECK(out.empty());

	CHECK(run({"ls", "-s", "bored"}) == 2);
	CHECK(err.find("error: unknown status 'bored'\n") == 0);
	CHECK(run({"ls", "odd", "taxi"}) == 2);
	CHECK(err.find("error: give one QUERY, after the options; quote a title of several words\n") ==
	      0);
	// Options come first.
	CHECK(run({"ls", "taxi", "-q"}) == 2);
	CHECK(out.empty());
}

TEST_CASE_FIXTURE(cliFixture, "cli ls sorts by title, or by the key asked for") {
	seed();
	CHECK(run({"ls", "-q", "--sort=title"}) == 0);
	CHECK(out == "m:1\na:2\na:1\nm:2\n");
	CHECK(run({"ls", "-qr"}) == 0);
	// Two titles that sort alike keep their order: anime, then manga.
	CHECK(out == "a:1\nm:2\na:2\nm:1\n");
	CHECK(run({"ls", "-q", "--sort=score"}) == 0);
	CHECK(out.find("a:1\na:2\n") == 0);
	CHECK(run({"ls", "-q", "--sort", "progress"}) == 0);
	CHECK(out.find("m:1\na:1\na:2\n") == 0);
	CHECK(run({"ls", "-qr", "--sort=progress"}) == 0);
	CHECK(out == "m:2\na:2\na:1\nm:1\n");
	CHECK(run({"ls", "-q", "--sort=new"}) == 0);
	CHECK(out.find("a:2\n") == 0);
	for (const char* key : {"updated", "date", "status"})
		CHECK(run({"ls", "-q", std::string("--sort=") + key}) == 0);

	CHECK(run({"ls", "--sort=colour"}) == 2);
	CHECK(err.find("error: unknown sort key 'colour'\n") == 0);
	CHECK(run({"ls", "--sort=Title"}) == 2);
}

TEST_CASE_FIXTURE(cliFixture, "cli ls --json prints one object a line") {
	seed();
	CHECK(run({"ls", "--json", "taxi"}) == 0);
	CHECK(out ==
	      "{\"ref\":\"a:2\",\"type\":\"anime\",\"title\":\"Odd Taxi\",\"status\":\"watching\","
	      "\"score\":8,\"progress\":5,\"total\":13,\"volumes\":0,\"rewatched\":0,"
	      "\"started\":\"2026-10-01\",\"finished\":null,\"updated\":null,\"new\":8}\n");
	// A total that is not known is null; --header is for the tabbed form.
	CHECK(run({"ls", "--json", "--header", "berserk"}) == 0);
	CHECK(out.find("{\"ref\":\"m:1\",\"type\":\"manga\",\"title\":\"Berserk\"") == 0);
	CHECK(has(out, "\"progress\":364,\"total\":null,"));
	CHECK(run({"ls", "--json"}) == 0);
	CHECK(std::count(out.begin(), out.end(), '\n') == 4);
	// The same at a terminal, and -q wins over it.
	CHECK(run({"ls", "--json", "taxi"}, "", 100) == 0);
	CHECK(out.find("{\"ref\":\"a:2\"") == 0);
	CHECK(run({"ls", "--json", "-q", "taxi"}) == 0);
	CHECK(out == "a:2\n");
}

TEST_CASE_FIXTURE(cliFixture, "cli ls keeps a tab or a quote in a title from breaking a line") {
	{
		catalist::catalogue db("data");
		db.addEntry(makeEntry().title("Tab\there, \"quoted\"").startDate("2026-01-02"));
		REQUIRE(db.saveAll());
	}
	CHECK(run({"ls"}) == 0);
	CHECK(out ==
	      "a:1\tanime\tTab here, \"quoted\"\twatching\t0\t0\t0\t0\t0\t2026-01-02\t-\t-\t0\n");
	CHECK(run({"ls", "--json"}) == 0);
	CHECK(has(out, "\"title\":\"Tab\\there, \\\"quoted\\\"\","));
	CHECK(has(out, "\"started\":\"2026-01-02\",\"finished\":null,"));
}

TEST_CASE_FIXTURE(cliFixture, "cli stops quietly when nothing reads its output any more") {
	seed();
	outClosed = true;
	for (const auto& args : {std::vector<std::string>{"ls"},
	                         {"ls", "-q"},
	                         {"ls", "--json"},
	                         {"show", "taxi", "berserk"},
	                         {"help"}}) {
		CHECK(run(args) == 0);
		CHECK(out.empty());
		CHECK(err.empty());
	}
	CHECK(run({"ls"}, "", 100) == 0);
	CHECK(run({"show", "--all", "frieren"}, "", 100) == 0);
	CHECK(err.empty());
}

TEST_CASE_FIXTURE(cliFixture, "cli ls draws a table at a terminal") {
	seed();
	CHECK(run({"ls"}, "", 100) == 0);
	CHECK(out ==
	      "REF  TITLE              STATUS        SCORE  PROGRESS  NEW\n"
	      "m:1  Berserk            Reading           -  364/?\n"
	      "a:2  Odd Taxi           Watching          8  5/13      +8\n"
	      "a:1  Sousou no Frieren  Completed        10  28/28\n"
	      "m:2  Sousou no Frieren  Plan to Read      -  0/?\n");
	CHECK(err.empty());

	// A narrow terminal cuts the titles, not the other columns.
	CHECK(run({"ls", "-a"}, "", 40) == 0);
	CHECK(out ==
	      "REF  TITLE       STATUS     SCORE  PROGRESS  NEW\n"
	      "a:2  Odd Taxi    Watching       8  5/13      +8\n"
	      "a:1  Sousou no…  Completed     10  28/28\n");

	CHECK(run({"ls", "nothing like it"}, "", 100) == 0);
	CHECK(out.empty());
	CHECK(err == "No titles to list.\n");
	CHECK(run({"ls", "-q", "taxi"}, "", 100) == 0);
	CHECK(out == "a:2\n");

	color = true;
	CHECK(run({"ls", "taxi"}, "", 100) == 0);
	CHECK(has(out, "\x1b[2mREF\x1b[0m  "));
	CHECK(has(out, "\x1b[36ma:2\x1b[0m  Odd Taxi  \x1b[32mWatching\x1b[0m  "));
	CHECK(has(out, "  \x1b[32m+8\x1b[0m\n"));
}

TEST_CASE_FIXTURE(cliFixture, "cli show prints a title's line into a pipe") {
	seed();
	CHECK(run({"show", "taxi"}) == 0);
	CHECK(
	    out ==
	    "a:2\tanime\tOdd Taxi\twatching\t8\t5\t13\t0\t0\t2026-10-01\t-\t-\t8\t0\t13\t128547\t44265\t"
	    "https://anilist.co/anime/128547\n");
	CHECK(err.empty());
	CHECK(run({"show", "--header", "m:1"}) == 0);
	CHECK(out ==
	      "ref\ttype\ttitle\tstatus\tscore\tprogress\ttotal\tvolumes\trewatched\tstarted\t"
	      "finished\tupdated\tnew\ttotal_volumes\treleased\tanilist_id\tmal_id\turl\n"
	      "m:1\tmanga\tBerserk\twatching\t0\t364\t0\t0\t0\t2019-05-01\t-\t-\t0\t0\t0\t30002\t2\t"
	      "https://anilist.co/manga/30002\n");
	CHECK(run({"show", "--json", "m:1"}) == 0);
	CHECK(out ==
	      "{\"ref\":\"m:1\",\"type\":\"manga\",\"title\":\"Berserk\",\"status\":\"watching\","
	      "\"score\":0,\"progress\":364,\"total\":null,\"volumes\":0,\"rewatched\":0,"
	      "\"started\":\"2019-05-01\",\"finished\":null,\"updated\":null,\"new\":0,"
	      "\"total_volumes\":null,\"released\":null,\"anilist_id\":30002,\"mal_id\":2,"
	      "\"url\":\"https://anilist.co/manga/30002\"}\n");
}

TEST_CASE_FIXTURE(cliFixture, "cli show takes several titles, by ref, by title or from a pipe") {
	seed();
	// Each once, in the order named.
	CHECK(run({"show", "a:1", "taxi", "a:1", "ODD"}) == 0);
	CHECK(out.find("a:1\t") == 0);
	CHECK(has(out, "\na:2\t"));
	CHECK(std::count(out.begin(), out.end(), '\n') == 2);
	CHECK(run({"show", "-"}, "a:2\n\nberserk\n") == 0);
	CHECK(out.find("a:2\t") == 0);
	CHECK(has(out, "\nm:1\t"));
	CHECK(run({"show", "m:2", "-"}, "a:2\n") == 0);
	CHECK(out.find("m:2\t") == 0);
	CHECK(has(out, "\na:2\t"));
	// Nothing in the pipe is nothing to do.
	CHECK(run({"show", "-"}) == 0);
	CHECK(out.empty());
	CHECK(err == "No titles named on standard input: nothing to do.\n");
	// A title that starts with a dash goes after --.
	CHECK(run({"show", "--", "-s"}) == 1);
	CHECK(has(err, "error: nothing in your lists matches '-s'\n"));

	CHECK(run({"show"}) == 2);
	CHECK(err.find("error: name a title: a ref such as a:12, or part of it\n") == 0);
	CHECK(run({"show", "taxi", ""}) == 2);
	CHECK(err.find("error: a REF cannot be empty\n") == 0);
	CHECK(out.empty());
}

TEST_CASE_FIXTURE(cliFixture, "cli show draws a card at a terminal") {
	seed();
	CHECK(run({"show", "taxi"}, "", 100) == 0);
	CHECK(out ==
	      "Odd Taxi\n"
	      "  Ref          a:2\n"
	      "  Type         Anime\n"
	      "  AniList      128547\n"
	      "  MyAnimeList  44265\n"
	      "  Page         https://anilist.co/anime/128547\n"
	      "  Status       Watching\n"
	      "  Score        8/10\n"
	      "  Progress     5/13 episodes  (+8 new)\n"
	      "  Rewatched    0\n"
	      "  Started      2026-10-01\n"
	      "  Finished     -\n"
	      "  Updated      -\n");
	CHECK(run({"show", "m:2", "berserk"}, "", 100) == 0);
	CHECK(out.find("Sousou no Frieren\n  Ref          m:2\n  Type         Manga\n") == 0);
	CHECK(has(out, "  Status       Plan to Read\n  Score        -\n"));
	CHECK(has(out, "  Progress     0/? chapters\n  Volumes      0/?\n  Reread       0\n"));
	// A blank line between two cards.
	CHECK(has(out, "  Updated      -\n\nBerserk\n"));
	CHECK(run({"show", "--json", "taxi"}, "", 100) == 0);
	CHECK(out.find("{\"ref\":\"a:2\"") == 0);
}

TEST_CASE_FIXTURE(cliFixture, "cli says which title it could not find, and shows none of them") {
	seed();
	CHECK(run({"show", "naruto"}) == 1);
	CHECK(err ==
	      "error: nothing in your lists matches 'naruto'\n"
	      "See what is there with: catalist ls\n");
	CHECK(run({"show", "-a", "berserk"}) == 1);
	CHECK(err.find("error: nothing in your anime list matches 'berserk'\n") == 0);
	CHECK(run({"show", "a:9"}) == 1);
	CHECK(err.find("error: no title in your lists has the ref a:9\n") == 0);
	CHECK(run({"show", "-m", "a:2"}) == 1);
	CHECK(err.find("error: no title in your manga list has the ref a:2\n") == 0);
	// One REF that fails stops them all.
	CHECK(run({"show", "taxi", "naruto"}) == 1);
	CHECK(out.empty());
	// An option after the first title is read as a title.
	CHECK(run({"show", "taxi", "--json"}) == 1);
	CHECK(err ==
	      "error: nothing in your lists matches '--json'\n"
	      "Options go before the first operand.\n"
	      "See what is there with: catalist ls\n");
}

TEST_CASE_FIXTURE(cliFixture, "cli asks which title was meant, or lists them into a pipe") {
	seed();
	CHECK(run({"show", "frieren"}) == 1);
	CHECK(err ==
	      "error: 'frieren' matches 2 titles:\n"
	      "  a:1  Sousou no Frieren\n"
	      "  m:2  Sousou no Frieren\n"
	      "Name one by its ref (-a and -m keep to one list), or pass --all for all of them.\n");
	CHECK(out.empty());
	CHECK(run({"show", "-m", "frieren"}) == 0);
	CHECK(out.find("m:2\t") == 0);
	CHECK(run({"show", "--all", "frieren"}) == 0);
	CHECK(out.find("a:1\t") == 0);
	CHECK(has(out, "\nm:2\t"));

	CHECK(run({"show", "frieren"}, "2\n") == 0);
	CHECK(err ==
	      "'frieren' matches 2 titles:\n"
	      "   1  a:1  Sousou no Frieren\n"
	      "   2  m:2  Sousou no Frieren\n"
	      "Which one? [1-2, Enter to cancel]: ");
	CHECK(out.find("m:2\t") == 0);
	for (const char* answer : {"\n", "7\n", "both\n"}) {
		CHECK(run({"show", "frieren"}, answer) == 1);
		CHECK(has(err, "]: Cancelled.\n"));
		CHECK(out.empty());
	}
	// Standard input that names the titles cannot answer as well.
	CHECK(run({"show", "-"}, "frieren\n") == 1);
	CHECK(err.find("error: 'frieren' matches 2 titles:\n") == 0);
}

TEST_CASE_FIXTURE(cliFixture, "cli + counts one more and says what changed") {
	seed();
	CHECK(run({"+", "taxi"}) == 0);
	CHECK(out == "a:2\tOdd Taxi\tprogress\t5\t6\n");
	CHECK(err.empty());
	CHECK(savedTitle(mediaType::kAnime, "Odd Taxi").progress == 6);
	CHECK(web.posted.empty());

	// At a terminal the line is for reading.
	CHECK(run({"+", "a:2"}, "", 100) == 0);
	CHECK(out == "Odd Taxi  6 → 7/13\n");
	CHECK(run({"inc", "-n", "2", "berserk"}, "", 100) == 0);
	CHECK(out == "Berserk  364 → 366/?\n");
	color = true;
	CHECK(run({"+", "a:2"}, "", 100) == 0);
	CHECK(out == "\x1b[1mOdd Taxi\x1b[0m  7 → 8/13\n");
	color = false;

	// The first one starts a title, the last one completes it.
	CHECK(run({"+", "m:2"}) == 0);
	CHECK(out ==
	      "m:2\tSousou no Frieren\tprogress\t0\t1\n"
	      "m:2\tSousou no Frieren\tstatus\tplanned\twatching\n"
	      "m:2\tSousou no Frieren\tstarted\t-\t2026-10-20\n");
	CHECK(run({"+", "-n5", "taxi"}, "", 100) == 0);
	CHECK(out ==
	      "Odd Taxi  8 → 13/13\n"
	      "Odd Taxi  status watching → completed\n"
	      "Odd Taxi  finished - → 2026-10-20\n");
	auto taxi = savedTitle(mediaType::kAnime, "Odd Taxi");
	CHECK(taxi.status == watchStatus::kCompleted);
	CHECK(catalist::formatDate(taxi.updatedAt) == "2026-10-20");
}

TEST_CASE_FIXTURE(cliFixture, "cli - counts one less") {
	seed();
	CHECK(run({"-", "taxi"}) == 0);
	CHECK(out == "a:2\tOdd Taxi\tprogress\t5\t4\n");
	CHECK(run({"dec", "-n", "3", "taxi"}) == 0);
	CHECK(out == "a:2\tOdd Taxi\tprogress\t4\t1\n");
	// Below the total a completed title is open again.
	CHECK(run({"-", "a:1"}) == 0);
	CHECK(has(out, "a:1\tSousou no Frieren\tprogress\t28\t27\n"));
	CHECK(has(out, "a:1\tSousou no Frieren\tstatus\tcompleted\twatching\n"));
	// A title named - is not one: the command's own name comes first.
	CHECK(run({"-", "--", "-n"}) == 1);
	CHECK(err.find("error: nothing in your lists matches '-n'\n") == 0);
}

TEST_CASE_FIXTURE(cliFixture, "cli + takes several titles, from standard input as well") {
	seed();
	CHECK(run({"ls", "-q", "-s", "watching"}) == 0);
	std::string watching = out;
	CHECK(watching == "m:1\na:2\n");
	CHECK(run({"+", "-"}, watching) == 0);
	CHECK(out ==
	      "m:1\tBerserk\tprogress\t364\t365\n"
	      "a:2\tOdd Taxi\tprogress\t5\t6\n");
	CHECK(err.empty());
	// One step for undo, named as the command was typed.
	auto history = steps();
	REQUIRE(history.size() == 1);
	CHECK(history[0].label == "+ -");
	CHECK(history[0].stamp == "2026-10-20 21:30:00");
	CHECK(history[0].changes == 2);

	CHECK(run({"+", "--all", "-q", "frieren", "taxi"}) == 0);
	CHECK(out.empty());
	CHECK(err.empty());
	CHECK(savedTitle(mediaType::kAnime, "Odd Taxi").progress == 7);
	CHECK(savedTitle(mediaType::kManga, "Sousou no Frieren").progress == 1);
	// The same title named twice is counted once.
	CHECK(run({"+", "taxi", "a:2"}) == 0);
	CHECK(out == "a:2\tOdd Taxi\tprogress\t7\t8\n");
}

TEST_CASE_FIXTURE(cliFixture, "cli + changes nothing when one title cannot be found") {
	seed();
	CHECK(run({"+", "taxi", "naruto"}) == 1);
	CHECK(err.find("error: nothing in your lists matches 'naruto'\n") == 0);
	CHECK(out.empty());
	CHECK(run({"+", "taxi", "frieren"}) == 1);
	CHECK(err.find("error: 'frieren' matches 2 titles:\n") == 0);
	CHECK(run({"+", "frieren"}, "7\n") == 1);
	CHECK(has(err, "Cancelled.\n"));
	CHECK(savedTitle(mediaType::kAnime, "Odd Taxi").progress == 5);
	CHECK(steps().empty());

	CHECK(run({"+"}) == 2);
	CHECK(err.find("error: ") == 0);
	CHECK(has(err, "Usage: catalist + [-amq] [--all] [-n N] REF...\n"));
	CHECK(run({"+", "-n", "0", "taxi"}) == 2);
	CHECK(err.find("error: -n '0' is not a count of one or more\n") == 0);
	CHECK(run({"+", "-n", "x", "taxi"}) == 2);
	CHECK(run({"+", "-n"}) == 2);
	CHECK(run({"+", "--by", "2", "taxi"}) == 2);
	CHECK(savedTitle(mediaType::kAnime, "Odd Taxi").progress == 5);
}

TEST_CASE_FIXTURE(cliFixture, "cli says when a command changed nothing, and records no step") {
	seed();
	// Frieren is complete: there is no more to count.
	CHECK(run({"+", "a:1"}) == 0);
	CHECK(out.empty());
	CHECK(err == "Nothing changed.\n");
	CHECK(run({"set", "taxi", "score=8", "progress=5"}) == 0);
	CHECK(err == "Nothing changed.\n");
	CHECK(run({"resume", "taxi"}) == 0);
	CHECK(err == "Nothing changed.\n");
	CHECK(run({"done", "a:1"}, "", 100) == 0);
	CHECK(err == "Nothing changed.\n");
	CHECK(out.empty());
	CHECK(steps().empty());
	// Only the titles that changed are printed.
	CHECK(run({"+", "--all", "frieren"}) == 0);
	CHECK(out.find("m:2\t") == 0);
	CHECK_FALSE(has(out, "a:1\t"));
}

TEST_CASE_FIXTURE(cliFixture, "cli set changes the fields of a title") {
	seed();
	CHECK(run({"set", "taxi", "score=9", "progress=13"}) == 0);
	CHECK(out ==
	      "a:2\tOdd Taxi\tprogress\t5\t13\n"
	      "a:2\tOdd Taxi\tstatus\twatching\tcompleted\n"
	      "a:2\tOdd Taxi\tscore\t8\t9\n"
	      "a:2\tOdd Taxi\tfinished\t-\t2026-10-20\n");
	CHECK(err.empty());
	auto taxi = savedTitle(mediaType::kAnime, "Odd Taxi");
	CHECK(taxi.status == watchStatus::kCompleted);
	CHECK(taxi.score == 9);
	CHECK(steps().size() == 1);

	CHECK(run({"set", "a:2", "title=ODDTAXI", "start=2026-09-30", "finish=", "rewatched=1"}, "",
	          100) == 0);
	CHECK(out ==
	      "ODDTAXI  rewatched 0 → 1\n"
	      "ODDTAXI  started 2026-10-01 → 2026-09-30\n"
	      "ODDTAXI  finished 2026-10-20 → -\n"
	      "ODDTAXI  title Odd Taxi → ODDTAXI\n");
	taxi = savedTitle(mediaType::kAnime, "ODDTAXI");
	CHECK(catalist::formatDate(taxi.startDate) == "2026-09-30");
	CHECK_FALSE(taxi.finishDate);
	CHECK(taxi.status == watchStatus::kCompleted);

	// The total first, so the progress can reach it; a field's name in any
	// case, and the names the columns of ls have for the dates.
	CHECK(run({"set", "-m", "berserk", "Progress=380", "total=380", "VOLUMES=42",
	           "started=2019-04-30", "finished=2026-10-19"}) == 0);
	auto berserk = savedTitle(mediaType::kManga, "Berserk");
	CHECK(berserk.totalUnits == 380);
	CHECK(berserk.progress == 380);
	CHECK(berserk.volumes == 42);
	CHECK(berserk.status == watchStatus::kCompleted);
	CHECK(catalist::formatDate(berserk.startDate) == "2019-04-30");
	CHECK(catalist::formatDate(berserk.finishDate) == "2026-10-19");

	// The status last, so a status that is given stands.
	CHECK(run({"set", "a:1", "status=on-hold", "progress=3", "score=0"}) == 0);
	auto frieren = savedTitle(mediaType::kAnime, "Sousou no Frieren");
	CHECK(frieren.status == watchStatus::kOnHold);
	CHECK(frieren.progress == 3);
	CHECK(frieren.score == 0);
	// A value may hold an equals sign, and a title spaces.
	CHECK(run({"set", "-q", "a:1", "title=1 + 1 = 2"}) == 0);
	CHECK(out.empty());
	CHECK(savedTitle(mediaType::kAnime, "1 + 1 = 2").id == 1);
	// Every title of that name.
	CHECK(run({"set", "--all", "1", "score=7"}) == 0);
	CHECK(has(out, "a:1\t1 + 1 = 2\tscore\t0\t7\n"));
}

TEST_CASE_FIXTURE(cliFixture, "cli set checks every value before touching anything") {
	seed();
	CHECK(run({"set"}) == 2);
	CHECK(err.find("error: name a title") == 0);
	CHECK(run({"set", "taxi"}) == 2);
	CHECK(err ==
	      "error: nothing to change: give a FIELD=VALUE such as score=8\n"
	      "Usage: catalist set [-amq] [--all] REF FIELD=VALUE...\n"
	      "Run 'catalist help set' for more.\n");
	CHECK(run({"set", "taxi", "progress=6", "score=11"}) == 2);
	CHECK(err.find("error: score '11' is not 0 to 10\n") == 0);
	CHECK(run({"set", "taxi", "progress=6", "status=bored"}) == 2);
	CHECK(err.find(
	          "error: status 'bored' is not watching, completed, on-hold, dropped or planned\n") ==
	      0);
	CHECK(run({"set", "taxi", "progress=lots"}) == 2);
	CHECK(err.find("error: progress 'lots' is not a count\n") == 0);
	CHECK(run({"set", "taxi", "progress=-1"}) == 2);
	CHECK(run({"set", "taxi", "progress=1234567"}) == 2);
	CHECK(run({"set", "taxi", "start=yesterday"}) == 2);
	CHECK(err.find("error: start 'yesterday' is not a date such as 2026-10-20, or empty") == 0);
	CHECK(run({"set", "taxi", "finish=2026-02-30"}) == 2);
	CHECK(run({"set", "taxi", "title="}) == 2);
	CHECK(err.find("error: title '' is not a title\n") == 0);
	CHECK(run({"set", "taxi", "scor=9"}) == 2);
	CHECK(err.find("error: unknown field 'scor' (did you mean 'score'?)\n") == 0);
	CHECK(run({"set", "taxi", "colour=red"}) == 2);
	CHECK(err.find("error: unknown field 'colour'\n") == 0);
	CHECK(run({"set", "taxi", "score=9", "nine"}) == 2);
	CHECK(err.find("error: 'nine' is not FIELD=VALUE") == 0);
	// The earlier form of the command.
	CHECK(run({"set", "taxi", "-p", "13"}) == 2);
	CHECK(out.empty());
	auto taxi = savedTitle(mediaType::kAnime, "Odd Taxi");
	CHECK(taxi.progress == 5);
	CHECK(taxi.score == 8);

	// The title is looked for once the values are known to be good.
	CHECK(run({"set", "naruto", "score=9"}) == 1);
	CHECK(err.find("error: nothing in your lists matches 'naruto'\n") == 0);
	CHECK(run({"set", "frieren", "score=9"}) == 1);
	CHECK(err.find("error: 'frieren' matches 2 titles:\n") == 0);
	CHECK(run({"set", "frieren", "score=9"}, "2\n") == 0);
	CHECK(out == "m:2\tSousou no Frieren\tscore\t0\t9\n");
	CHECK(steps().size() == 1);
}

TEST_CASE_FIXTURE(cliFixture, "cli done, hold, drop, plan and resume set the status") {
	seed();
	CHECK(run({"done", "--score=9", "taxi"}) == 0);
	CHECK(out ==
	      "a:2\tOdd Taxi\tprogress\t5\t13\n"
	      "a:2\tOdd Taxi\tstatus\twatching\tcompleted\n"
	      "a:2\tOdd Taxi\tscore\t8\t9\n"
	      "a:2\tOdd Taxi\tfinished\t-\t2026-10-20\n");
	CHECK(err.empty());
	CHECK(run({"hold", "berserk"}, "", 100) == 0);
	CHECK(out ==
	      "Berserk  status watching → on-hold\n"
	      "Berserk  finished - → 2026-10-20\n");
	CHECK(savedTitle(mediaType::kManga, "Berserk").status == watchStatus::kOnHold);
	CHECK(run({"resume", "berserk"}) == 0);
	CHECK(out == "m:1\tBerserk\tstatus\ton-hold\twatching\n");
	CHECK(run({"drop", "-q", "m:1"}) == 0);
	CHECK(out.empty());
	CHECK(savedTitle(mediaType::kManga, "Berserk").status == watchStatus::kDropped);
	CHECK(run({"plan", "-"}, "m:1\n\na:2\n") == 0);
	CHECK(has(out, "m:1\tBerserk\tstatus\tdropped\tplanned\n"));
	CHECK(has(out, "a:2\tOdd Taxi\tstatus\tcompleted\tplanned\n"));
	CHECK(steps().size() == 5);
	CHECK(steps()[0].label == "plan -");
	CHECK(steps()[0].changes == 2);
	// A manga of unknown length is done where it stands.
	CHECK(run({"done", "--score", "0", "--all", "frieren"}) == 0);
	CHECK(out ==
	      "a:1\tSousou no Frieren\tscore\t10\t0\n"
	      "m:2\tSousou no Frieren\tstatus\tplanned\tcompleted\n"
	      "m:2\tSousou no Frieren\tstarted\t-\t2026-10-20\n"
	      "m:2\tSousou no Frieren\tfinished\t-\t2026-10-20\n");

	CHECK(run({"done", "--score=11", "taxi"}) == 2);
	CHECK(err.find("error: --score '11' is not 0 to 10\n") == 0);
	CHECK(run({"done", "--score=high", "taxi"}) == 2);
	CHECK(run({"hold", "--score=3", "taxi"}) == 2);
	CHECK(err.find("error: unknown option '--score'\n") == 0);
	CHECK(run({"drop"}) == 2);
	CHECK(run({"drop", "naruto"}) == 1);
	CHECK(run({"drop", "frieren"}) == 1);
	CHECK(steps().size() == 6);
}

TEST_CASE_FIXTURE(cliFixture, "cli undo takes back the last command, and redo makes it again") {
	seed();
	CHECK(run({"undo"}) == 0);
	CHECK(err == "Nothing to undo.\n");
	CHECK(out.empty());
	CHECK(run({"redo"}) == 0);
	CHECK(err == "Nothing to redo.\n");

	REQUIRE(run({"+", "taxi"}) == 0);
	REQUIRE(run({"done", "--score=9", "taxi"}) == 0);
	CHECK(run({"undo"}) == 0);
	CHECK(err == "Undid 'done --score=9 taxi' (2026-10-20 21:30:00).\n");
	CHECK(out ==
	      "a:2\tOdd Taxi\tprogress\t13\t6\n"
	      "a:2\tOdd Taxi\tstatus\tcompleted\twatching\n"
	      "a:2\tOdd Taxi\tscore\t9\t8\n"
	      "a:2\tOdd Taxi\tfinished\t2026-10-20\t-\n");
	auto taxi = savedTitle(mediaType::kAnime, "Odd Taxi");
	CHECK(taxi.progress == 6);
	CHECK(taxi.status == watchStatus::kWatching);
	CHECK_FALSE(taxi.finishDate);

	CHECK(run({"redo"}, "", 100) == 0);
	CHECK(err == "Redid 'done --score=9 taxi' (2026-10-20 21:30:00).\n");
	CHECK(out.find("Odd Taxi  6 → 13/13\nOdd Taxi  status watching → completed\n") == 0);
	CHECK(savedTitle(mediaType::kAnime, "Odd Taxi").score == 9);
	CHECK(run({"redo"}) == 0);
	CHECK(err == "Nothing to redo.\n");

	// Several steps, newest first; what they did between them is printed once.
	CHECK(run({"undo", "-n", "2"}) == 0);
	CHECK(err ==
	      "Undid 'done --score=9 taxi' (2026-10-20 21:30:00).\n"
	      "Undid '+ taxi' (2026-10-20 21:30:00).\n");
	CHECK(has(out, "a:2\tOdd Taxi\tprogress\t13\t5\n"));
	CHECK(savedTitle(mediaType::kAnime, "Odd Taxi").progress == 5);
	CHECK(run({"redo", "-qn9"}) == 0);
	CHECK(out.empty());
	CHECK(err.empty());
	CHECK(savedTitle(mediaType::kAnime, "Odd Taxi").progress == 13);

	// A new change ends what could be redone.
	REQUIRE(run({"undo"}) == 0);
	REQUIRE(run({"hold", "berserk"}) == 0);
	CHECK(run({"redo"}) == 0);
	CHECK(err == "Nothing to redo.\n");

	CHECK(run({"undo", "taxi"}) == 2);
	CHECK(err.find("error: undo takes no operands\n") == 0);
	CHECK(run({"undo", "-n", "0"}) == 2);
	CHECK(run({"redo", "-n", "many"}) == 2);
	CHECK(run({"redo", "--all"}) == 2);
}

TEST_CASE_FIXTURE(cliFixture, "cli undo brings a removed title back under its ref") {
	seed();
	REQUIRE(run({"rm", "taxi", "--yes"}) == 0);
	CHECK(saved(mediaType::kAnime).size() == 1);
	CHECK(run({"undo"}) == 0);
	CHECK(err == "Undid 'rm taxi --yes' (2026-10-20 21:30:00).\n");
	CHECK(out == "a:2\tOdd Taxi\tadded\t-\twatching\n");
	auto taxi = savedTitle(mediaType::kAnime, "Odd Taxi");
	CHECK(taxi.id == 2);
	CHECK(taxi.anilistId == 128547);
	CHECK(taxi.progress == 5);
	CHECK(run({"redo"}, "", 100) == 0);
	CHECK(out == "Odd Taxi  removed\n");
	CHECK(saved(mediaType::kAnime).size() == 1);
	CHECK(run({"undo"}, "", 100) == 0);
	CHECK(out == "Odd Taxi  added as watching\n");
}

TEST_CASE_FIXTURE(cliFixture, "cli undo changes nothing when a title has changed since") {
	seed();
	REQUIRE(run({"+", "taxi"}) == 0);
	REQUIRE(run({"+", "berserk"}) == 0);
	changeBehindTheHistory(mediaType::kAnime, "Odd Taxi", 9);

	CHECK(run({"undo", "-n", "2"}) == 1);
	CHECK(err ==
	      "error: cannot undo '+ taxi' (2026-10-20 21:30:00): Odd Taxi (a:2) has changed since: "
	      "progress is now 9, not 6\n"
	      "Nothing was changed. With -f the step is taken all the same, where it still can "
	      "be.\n");
	CHECK(out.empty());
	CHECK(savedTitle(mediaType::kManga, "Berserk").progress == 365);
	CHECK(savedTitle(mediaType::kAnime, "Odd Taxi").progress == 9);
	CHECK_FALSE(steps()[0].undone);

	CHECK(run({"undo", "-f", "-n", "2"}) == 0);
	CHECK(has(out, "a:2\tOdd Taxi\tprogress\t9\t5\n"));
	CHECK(savedTitle(mediaType::kManga, "Berserk").progress == 364);
	CHECK(savedTitle(mediaType::kAnime, "Odd Taxi").progress == 5);

	// The step undone last is the first to be made again.
	changeBehindTheHistory(mediaType::kAnime, "Odd Taxi", 1);
	CHECK(run({"redo"}) == 1);
	CHECK(err.find("error: cannot redo '+ taxi' (2026-10-20 21:30:00): Odd Taxi (a:2) has "
	               "changed since: progress is now 1, not 5\n") == 0);
	CHECK(run({"redo", "-f"}) == 0);
	CHECK(savedTitle(mediaType::kAnime, "Odd Taxi").progress == 6);
	CHECK(savedTitle(mediaType::kManga, "Berserk").progress == 364);
}

TEST_CASE_FIXTURE(cliFixture, "cli log lists the steps, newest first") {
	seed();
	CHECK(run({"log"}) == 0);
	CHECK(out.empty());
	CHECK(err.empty());
	CHECK(run({"log"}, "", 100) == 0);
	CHECK(out.empty());
	CHECK(err == "Nothing has been changed yet.\n");

	REQUIRE(run({"+", "--all", "frieren", "taxi"}) == 0);
	REQUIRE(run({"set", "a:2", "score=9"}) == 0);
	REQUIRE(run({"hold", "berserk"}) == 0);
	REQUIRE(run({"undo"}) == 0);
	CHECK(run({"log"}) == 0);
	CHECK(out ==
	      "3\t2026-10-20 21:30:00\tundone\t1\thold berserk\n"
	      "2\t2026-10-20 21:30:00\tdone\t1\tset a:2 score=9\n"
	      "1\t2026-10-20 21:30:00\tdone\t2\t+ --all frieren taxi\n");
	CHECK(err.empty());
	CHECK(run({"log", "--header", "-n", "1"}) == 0);
	CHECK(out ==
	      "tx\tstamp\tstate\tchanges\tlabel\n"
	      "3\t2026-10-20 21:30:00\tundone\t1\thold berserk\n");
	CHECK(run({"log", "--json", "-n2"}) == 0);
	CHECK(out ==
	      "{\"tx\":3,\"stamp\":\"2026-10-20 21:30:00\",\"state\":\"undone\",\"changes\":1,"
	      "\"label\":\"hold berserk\"}\n"
	      "{\"tx\":2,\"stamp\":\"2026-10-20 21:30:00\",\"state\":\"done\",\"changes\":1,"
	      "\"label\":\"set a:2 score=9\"}\n");
	CHECK(run({"log"}, "", 100) == 0);
	CHECK(out ==
	      "TX  WHEN                 STATE   CHANGES  COMMAND\n"
	      " 3  2026-10-20 21:30:00  undone        1  hold berserk\n"
	      " 2  2026-10-20 21:30:00  done          1  set a:2 score=9\n"
	      " 1  2026-10-20 21:30:00  done          2  + --all frieren taxi\n");

	// A long command is cut to 60 characters, never inside a character.
	REQUIRE(run({"set", "a:2", "title=" + std::string(40, 'x') + "葬送のフリーレン"}) == 0);
	CHECK(steps()[0].label == "set a:2 title=" + std::string(40, 'x') + "葬送のフリー");

	CHECK(run({"log", "taxi"}) == 2);
	CHECK(run({"log", "-n", "0"}) == 2);
	// Nothing is stored yet: nothing to list, and nothing is created.
	std::filesystem::remove_all("data");
	CHECK(run({"log"}) == 0);
	CHECK(out.empty());
	CHECK_FALSE(std::filesystem::exists("data"));
}

TEST_CASE_FIXTURE(cliFixture, "cli tx prints a label, and commands run under one undo as one") {
	seed();
	CHECK(run({"tx"}) == 0);
	CHECK(out == "1792531800-4242\n");
	CHECK(err.empty());
	CHECK(run({"tx", "now"}) == 2);

	REQUIRE(run({"+", "taxi"}) == 0);
	group = "1792531800-4242";
	REQUIRE(run({"+", "taxi"}) == 0);
	REQUIRE(run({"+", "-"}, "a:2\nm:1\n") == 0);
	REQUIRE(run({"done", "m:2"}) == 0);
	REQUIRE(run({"rm", "manga", "berserk", "--yes"}) == 0);
	group.clear();

	auto history = steps();
	REQUIRE(history.size() == 2);
	CHECK(history[0].label == "1792531800-4242");
	CHECK(history[0].changes == 3);
	CHECK(history[1].label == "+ taxi");

	CHECK(run({"undo"}) == 0);
	CHECK(err == "Undid '1792531800-4242' (2026-10-20 21:30:00).\n");
	CHECK(has(out, "a:2\tOdd Taxi\tprogress\t8\t6\n"));
	CHECK(has(out, "m:1\tBerserk\tadded\t-\twatching\n"));
	CHECK(savedTitle(mediaType::kAnime, "Odd Taxi").progress == 6);
	CHECK(savedTitle(mediaType::kManga, "Berserk").progress == 364);
	CHECK(savedTitle(mediaType::kManga, "Sousou no Frieren").status == watchStatus::kPlanToWatch);
	CHECK(run({"redo"}) == 0);
	CHECK(saved(mediaType::kManga).size() == 1);
	CHECK(savedTitle(mediaType::kAnime, "Odd Taxi").progress == 8);
}

TEST_CASE_FIXTURE(cliFixture, "cli stops quietly when nobody reads what a change printed") {
	seed();
	outClosed = true;
	CHECK(run({"+", "--all", "frieren", "taxi"}) == 0);
	CHECK(err.empty());
	CHECK(savedTitle(mediaType::kAnime, "Odd Taxi").progress == 6);
	CHECK(run({"undo"}) == 0);
	CHECK(run({"log"}) == 0);
	CHECK(run({"tx"}) == 0);
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
	      "Change it with: catalist set a:3 ...\n");
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
	{
		catalist::catalogue db("data");
		REQUIRE(db.loadAll().ok());
		CHECK(db.getProvider() == catalist::provider::kMyAnimeList);
	}
	CHECK(run({"show", "taxi"}) == 0);
	CHECK(has(out, "\thttps://myanimelist.net/anime/44265\n"));
	CHECK(run({"show", "taxi"}, "", 100) == 0);
	CHECK(has(out, "  Page         https://myanimelist.net/anime/44265\n"));

	CHECK(run({"provider", "kitsu"}) == 2);
	CHECK(run({"provider", "anilist"}) == 0);
	CHECK(run({"show", "taxi"}) == 0);
	CHECK(has(out, "\thttps://anilist.co/anime/128547\n"));
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
	CHECK(saved(mediaType::kAnime).empty());

	put("lists/anime.xml.gz", std::string("\x1f\x8b\x08\x00", 4) + "gz");
	CHECK(run({"import", "lists/anime.xml.gz"}) == 1);
	CHECK(err ==
	      "error: lists/anime.xml.gz is compressed; unpack it first "
	      "(gunzip lists/anime.xml.gz)\n");

	CHECK(run({"import", "lists/anime.xml", "lists/empty.xml"}) == 1);
	CHECK(err ==
	      "error: no anime or manga found in lists/empty.xml (is it a "
	      "MyAnimeList XML export?)\n");
	CHECK(saved(mediaType::kAnime).empty());
}

TEST_CASE_FIXTURE(cliFixture, "cli import exits 1 when the catalogue cannot be saved") {
	put("lists/anime.xml", kAnimeXml);
	catalist::test::makeUnwritable("data/catalist.db");
	CHECK(run({"import", "lists/anime.xml"}) == 1);
	CHECK(err.find("error: cannot open data/catalist.db: ") == 0);
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
