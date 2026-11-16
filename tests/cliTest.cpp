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
	CHECK(run({"help", "add"}) == 0);
	CHECK(
	    has(out, "Usage: catalist add [-amq] [--pick=N] [--status=STATUS] [--no-lookup] TITLE\n"));
	// The helps line up past the longest option.
	CHECK(has(out, "      --status=STATUS  watching, completed, on-hold, dropped or planned\n"));
	CHECK(has(out, "      --no-lookup      add TITLE as written, without asking AniList\n"));
	CHECK(has(out, "  -q                   print nothing\n"));
	CHECK(run({"help", "rm"}) == 0);
	CHECK(has(out, "Usage: catalist rm [-amfq] [--all] REF...\n"));
	CHECK(run({"help", "search"}) == 0);
	CHECK(has(out, "Usage: catalist search [-am] [--json] [--header] QUERY\n"));
	CHECK(has(out, "  pick type title year total score anilist_id mal_id ref\n"));
	CHECK(run({"help", "stats"}) == 0);
	CHECK(has(out, "Usage: catalist stats [-am] [--json] [--header]\n"));
	CHECK(run({"help", "import"}) == 0);
	CHECK(has(out, "Usage: catalist import [-q] FILE...\n"));
	CHECK(run({"help", "export"}) == 0);
	CHECK(has(out, "Usage: catalist export [--format=FORMAT] anime|manga\n"));
	CHECK(run({"help", "provider"}) == 0);
	CHECK(has(out, "Usage: catalist provider [anilist|mal]\n"));

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
	for (const char* gone : {"list", "info", "version", "next", "edit", "remove", "delete"})
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

	CHECK(run({"add", "--stauts", "watching", "frieren"}) == 2);
	CHECK(err.find("error: unknown option '--stauts' (did you mean '--status'?)\n"
	               "Usage: catalist add ") == 0);
	CHECK(has(err, "Run 'catalist help add' for more.\n"));
	CHECK(run({"add", "--pick"}) == 2);
	CHECK(err.find("error: option '--pick' needs a value (N)\n") == 0);
	CHECK(run({"rm", "--yes", "taxi"}) == 2);
	CHECK(run({"import"}) == 2);
	CHECK(err.find("error: give at least one file, or - for standard input\n") == 0);
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
	// -q prints nothing, not even that.
	CHECK(run({"+", "-q", "a:1"}) == 0);
	CHECK(err.empty());
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
	REQUIRE(run({"rm", "-f", "taxi"}) == 0);
	CHECK(saved(mediaType::kAnime).size() == 1);
	CHECK(run({"undo"}) == 0);
	CHECK(err == "Undid 'rm -f taxi' (2026-10-20 21:30:00).\n");
	CHECK(out == "a:2\tOdd Taxi\tadded\t-\twatching\n");
	auto taxi = savedTitle(mediaType::kAnime, "Odd Taxi");
	CHECK(taxi.id == 2);
	CHECK(taxi.anilistId == 128547);
	CHECK(taxi.progress == 5);
	CHECK(run({"redo"}, "", 100) == 0);
	CHECK(out == "Odd Taxi  removed\n");
	CHECK(saved(mediaType::kAnime).size() == 1);
	CHECK(run({"undo"}, "", 100) == 0);
	CHECK(out == "Odd Taxi  added as watching (a:2)\n");
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
	REQUIRE(run({"rm", "-fm", "berserk"}) == 0);
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

TEST_CASE_FIXTURE(cliFixture, "cli rm asks first, and wants -f where nobody can be asked") {
	seed();
	CHECK(run({"rm", "taxi"}) == 1);
	CHECK(err == "error: nobody to ask here: pass -f to remove Odd Taxi (a:2)\n");
	CHECK(run({"rm", "taxi"}, "n\n") == 1);
	CHECK(err == "Remove Odd Taxi (a:2) from your lists? [y/N]: Cancelled.\n");
	CHECK(out.empty());
	CHECK(saved(mediaType::kAnime).size() == 2);
	CHECK(steps().empty());

	CHECK(run({"rm", "taxi"}, "y\n", 100) == 0);
	CHECK(out == "Odd Taxi  removed\n");
	CHECK(run({"rm", "-f", "a:1"}) == 0);
	CHECK(out == "a:1\tSousou no Frieren\tremoved\tcompleted\t-\n");
	CHECK(err.empty());
	CHECK(saved(mediaType::kAnime).empty());
	CHECK(saved(mediaType::kManga).size() == 2);
}

TEST_CASE_FIXTURE(cliFixture, "cli rm removes several titles as one step") {
	seed();
	// One question for all of them.
	CHECK(run({"rm", "--all", "frieren"}, "yes\n") == 0);
	CHECK(has(err, "  a:1  Sousou no Frieren\n"));
	CHECK(has(err, "  m:2  Sousou no Frieren\n"));
	CHECK(has(err, "Remove these 2 titles from your lists? [y/N]: "));
	CHECK(saved(mediaType::kAnime).size() == 1);
	CHECK(saved(mediaType::kManga).size() == 1);
	REQUIRE(steps().size() == 1);
	CHECK(steps()[0].changes == 2);

	// REFs from standard input leave nobody to ask.
	CHECK(run({"rm", "-"}, "a:2\nm:1\n") == 1);
	CHECK(has(err, "error: nobody to ask here: pass -f to remove these 2 titles\n"));
	CHECK(saved(mediaType::kAnime).size() == 1);
	CHECK(run({"rm", "-fq", "-"}, "a:2\nm:1\n") == 0);
	CHECK(out.empty());
	CHECK(err.empty());
	CHECK(saved(mediaType::kAnime).empty());
	CHECK(saved(mediaType::kManga).empty());
}

TEST_CASE_FIXTURE(cliFixture, "cli rm removes nothing when a REF is wrong") {
	seed();
	CHECK(run({"rm", "-f", "taxi", "nope"}) == 1);
	CHECK(saved(mediaType::kAnime).size() == 2);
	// Two titles are called Frieren.
	CHECK(run({"rm", "-f", "frieren"}) == 1);
	CHECK(saved(mediaType::kAnime).size() == 2);
	CHECK(saved(mediaType::kManga).size() == 2);
	CHECK(run({"rm"}) == 2);
	CHECK(run({"rm", "-f"}) == 2);
	CHECK(out.empty());
}

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
	CHECK(err.find("error: name one title; quote a title of several words\n"
	               "Usage: catalist add ") == 0);
	CHECK(run({"add", "odd", "taxi"}) == 2);
	CHECK(run({"add", ""}) == 2);
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
	CHECK(err.find("error: give one QUERY; quote a title of several words\n"
	               "Usage: catalist search ") == 0);
	CHECK(run({"search", "odd", "taxi"}) == 2);
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
	// Searching writes nothing, not even the data directory.
	CHECK_FALSE(std::filesystem::exists("data"));
}

TEST_CASE_FIXTURE(cliFixture, "cli stats prints one line of numbers a list") {
	seed();
	CHECK(run({"stats"}) == 0);
	CHECK(out ==
	      "anime\t2\t1\t1\t0\t0\t0\t9.00\t33\t0\t0\t0.6\n"
	      "manga\t2\t1\t0\t0\t0\t1\t0\t364\t0\t0\t1.3\n");
	CHECK(err.empty());
	CHECK(run({"stats", "-m", "--header"}) == 0);
	CHECK(out ==
	      "type\ttotal\twatching\tcompleted\ton_hold\tdropped\tplanned\tmean\tunits\tvolumes\t"
	      "rewatched\tdays\n"
	      "manga\t2\t1\t0\t0\t0\t1\t0\t364\t0\t0\t1.3\n");
	CHECK(run({"stats", "-a", "--json"}, "", 100) == 0);
	CHECK(out ==
	      "{\"type\":\"anime\",\"total\":2,\"watching\":1,\"completed\":1,\"on_hold\":0,"
	      "\"dropped\":0,\"planned\":0,\"mean\":9.00,\"units\":33,\"volumes\":0,"
	      "\"rewatched\":0,\"days\":0.6}\n");
	// No score given yet: there is no mean.
	CHECK(run({"stats", "-m", "--json"}) == 0);
	CHECK(has(out, "\"planned\":1,\"mean\":null,\"units\":364,"));
	CHECK(run({"stats", "anime"}) == 2);
	CHECK(err.find("error: stats takes no operands\nUsage: catalist stats ") == 0);
}

TEST_CASE_FIXTURE(cliFixture, "cli stats draws a card a list at a terminal") {
	seed();
	CHECK(run({"stats"}, "", 100) == 0);
	CHECK(out ==
	      "Anime\n"
	      "  Titles         2\n"
	      "  Watching       1\n"
	      "  Completed      1\n"
	      "  On-Hold        0\n"
	      "  Dropped        0\n"
	      "  Plan to Watch  0\n"
	      "  Mean score     9.00\n"
	      "  Episodes       33\n"
	      "  Rewatched      0\n"
	      "  Days spent     0.6\n"
	      "\n"
	      "Manga\n"
	      "  Titles         2\n"
	      "  Reading        1\n"
	      "  Completed      0\n"
	      "  On-Hold        0\n"
	      "  Dropped        0\n"
	      "  Plan to Read   1\n"
	      "  Mean score     -\n"
	      "  Chapters       364\n"
	      "  Volumes        0\n"
	      "  Reread         0\n"
	      "  Days spent     1.3\n");
	CHECK(run({"stats", "-m"}, "", 100) == 0);
	CHECK(out.find("Manga\n") == 0);
}

TEST_CASE_FIXTURE(cliFixture, "cli stats of an empty catalogue is all zeros, and writes nothing") {
	CHECK(run({"stats"}) == 0);
	CHECK(out ==
	      "anime\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0.0\n"
	      "manga\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0.0\n");
	CHECK_FALSE(std::filesystem::exists("data"));
}

TEST_CASE_FIXTURE(cliFixture, "cli provider prints or sets the site titles are looked at on") {
	seed();
	CHECK(run({"provider"}) == 0);
	CHECK(out == "anilist\n");
	CHECK(err.empty());
	CHECK(run({"provider", "mal"}) == 0);
	CHECK(out.empty());
	CHECK(err == "Your provider is now MyAnimeList.\n");
	{
		catalist::catalogue db("data");
		REQUIRE(db.loadAll().ok());
		CHECK(db.getProvider() == catalist::provider::kMyAnimeList);
	}
	CHECK(run({"provider"}) == 0);
	CHECK(out == "mal\n");
	CHECK(run({"show", "taxi"}) == 0);
	CHECK(has(out, "\thttps://myanimelist.net/anime/44265\n"));
	CHECK(run({"show", "taxi"}, "", 100) == 0);
	CHECK(has(out, "  Page         https://myanimelist.net/anime/44265\n"));
	CHECK(run({"provider", "myanimelist"}) == 0);
	CHECK(err == "Nothing changed.\n");

	CHECK(run({"provider", "kitsu"}) == 2);
	CHECK(err.find("error: unknown provider 'kitsu'; choose anilist or mal\n"
	               "Usage: catalist provider [anilist|mal]\n") == 0);
	CHECK(run({"provider", "anilist", "mal"}) == 2);
	CHECK(run({"provider", "anilist"}) == 0);
	CHECK(run({"show", "taxi"}) == 0);
	CHECK(has(out, "\thttps://anilist.co/anime/128547\n"));
	// A setting is not a step for undo.
	CHECK(steps().empty());
}

TEST_CASE_FIXTURE(cliFixture, "cli import adds the titles and saves them") {
	put("lists/anime.xml", kAnimeXml);
	put("lists/manga.xml", kMangaXml);

	CHECK(run({"import", "lists/anime.xml", "lists/manga.xml"}) == 0);
	CHECK(out ==
	      "a:1\tSousou no Frieren\tadded\t-\tcompleted\n"
	      "a:2\tOdd Taxi\tadded\t-\twatching\n"
	      "m:1\tBerserk\tadded\t-\twatching\n");
	CHECK(err ==
	      "lists/anime.xml: 2 titles, 2 added, 0 in your lists already\n"
	      "lists/manga.xml: 1 title, 1 added, 0 in your lists already\n");

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

	// All the files of one command are one step for undo.
	REQUIRE(steps().size() == 1);
	CHECK(steps()[0].label == "import lists/anime.xml lists/manga.xml");
	CHECK(steps()[0].changes == 3);
	CHECK(run({"undo", "-q"}) == 0);
	CHECK(saved(mediaType::kAnime).empty());
	CHECK(saved(mediaType::kManga).empty());
}

TEST_CASE_FIXTURE(cliFixture, "cli import reads standard input for a FILE of -") {
	CHECK(run({"import", "-"}, kAnimeXml) == 0);
	CHECK(has(out, "\tOdd Taxi\tadded\t-\twatching\n"));
	CHECK(err == "standard input: 2 titles, 2 added, 0 in your lists already\n");
	CHECK(saved(mediaType::kAnime).size() == 2);
	// -q prints nothing, not even that.
	CHECK(run({"import", "-q", "-"}, kAnimeXml) == 0);
	CHECK(out.empty());
	CHECK(err.empty());
	CHECK(run({"import", "-"}, "not an export") == 1);
	CHECK(err ==
	      "error: no anime or manga found in standard input (is it a MyAnimeList XML export?)\n");
	CHECK(run({"import", "-"}, std::string("\x1f\x8b\x08\x00", 4) + "gz") == 1);
	CHECK(err == "error: standard input is compressed; unpack it first (gunzip)\n");
}

TEST_CASE_FIXTURE(cliFixture, "cli import has the tracker look the new titles up") {
	seed();
	put("lists/anime.xml", kAnimeXml);
	// Both titles are there already: nothing new to look up.
	CHECK(run({"import", "lists/anime.xml"}) == 0);
	{
		catalist::catalogue db("data");
		REQUIRE(db.loadAll().ok());
		CHECK(db.getLinkedOn() == testToday());
	}
	put("lists/manga.xml",
	    "<myanimelist><manga><manga_mangadb_id>656</manga_mangadb_id>"
	    "<manga_title><![CDATA[Vagabond]]></manga_title>"
	    "<my_status>Plan to Read</my_status></manga></myanimelist>");
	CHECK(run({"import", "lists/manga.xml"}) == 0);
	CHECK(out == "m:3\tVagabond\tadded\t-\tplanned\n");
	catalist::catalogue db("data");
	REQUIRE(db.loadAll().ok());
	CHECK_FALSE(db.getLinkedOn());
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
	CHECK(out.empty());
	CHECK(err ==
	      "lists/anime.xml: 2 titles, 0 added, 2 in your lists already\n"
	      "Nothing changed.\n");
	auto anime = saved(mediaType::kAnime);
	REQUIRE(anime.size() == 2);
	CHECK(anime[1].score == 9);
	CHECK(anime[1].progress == 13);
	CHECK(catalist::formatDate(anime[1].startDate) == "2026-01-02");
	CHECK(steps().size() == 1);
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
	CHECK(run({"add", "--no-lookup", "Home Video"}) == 1);
	CHECK(err.find("error: cannot open data/catalist.db: ") == 0);
	CHECK(out.empty());
}

TEST_CASE_FIXTURE(cliFixture, "cli refuses to work on a catalogue it cannot read") {
	put("data/anime.csv", "this,is,not,a,catalogue\n");
	put("lists/anime.xml", kAnimeXml);
	CHECK(run({"import", "lists/anime.xml"}) == 1);
	CHECK(err.find("error: data/anime.csv: ") == 0);
	for (const char* command : {"stats", "provider"})
		CHECK(run({command}) == 1);
	CHECK(run({"export", "anime"}) == 1);
	CHECK(run({"rm", "-f", "taxi"}) == 1);
	CHECK(run({"add", "--no-lookup", "Home Video"}) == 1);
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
	// xml is the format when none is named.
	std::string xml = out;
	CHECK(run({"export", "--format=xml", "anime"}) == 0);
	CHECK(out == xml);

	CHECK(run({"export", "manga"}) == 0);
	auto manga = catalist::malXml::parse(out);
	REQUIRE(manga.size() == 1);
	CHECK(manga[0].type == mediaType::kManga);
	CHECK(manga[0].title == "Berserk");
}

TEST_CASE_FIXTURE(cliFixture, "cli export --format=csv prints the columns of ls, quoted as CSV") {
	seed();
	REQUIRE(run({"add", "--no-lookup", "Say \"hi\", twice"}) == 0);
	CHECK(run({"export", "--format=csv", "anime"}) == 0);
	CHECK(err.empty());
	CHECK(out ==
	      "ref,type,title,status,score,progress,total,volumes,rewatched,started,finished,"
	      "updated,new\n"
	      "a:1,anime,Sousou no Frieren,completed,10,28,28,0,0,,,,0\n"
	      "a:2,anime,Odd Taxi,watching,8,5,13,0,0,2026-10-01,,,8\n"
	      "a:3,anime,\"Say \"\"hi\"\", twice\",planned,0,0,,0,0,,,2026-10-20,0\n");
	// The same at a terminal: a file is what it is for.
	std::string csv = out;
	CHECK(run({"export", "--format", "csv", "anime"}, "", 100) == 0);
	CHECK(out == csv);
	CHECK(run({"export", "--format=csv", "manga"}) == 0);
	CHECK(std::count(out.begin(), out.end(), '\n') == 3);
}

TEST_CASE_FIXTURE(cliFixture, "cli export --format=json prints one object a title") {
	seed();
	CHECK(run({"export", "--format=json", "anime"}) == 0);
	CHECK(std::count(out.begin(), out.end(), '\n') == 2);
	CHECK(out.find("{\"ref\":\"a:1\",\"type\":\"anime\",\"title\":\"Sousou no Frieren\",") == 0);
	CHECK(
	    has(out,
	        "\n{\"ref\":\"a:2\",\"type\":\"anime\",\"title\":\"Odd Taxi\",\"status\":\"watching\","
	        "\"score\":8,\"progress\":5,\"total\":13,"));
	// The lines of `ls --json`.
	std::string json = out;
	CHECK(run({"ls", "-a", "--sort=updated", "--json"}) == 0);
	CHECK(out.size() == json.size());
}

TEST_CASE_FIXTURE(cliFixture, "cli export wants a list and a format it knows") {
	seed();
	CHECK(run({"export"}) == 2);
	CHECK(err.find("error: say which list: anime or manga\nUsage: catalist export ") == 0);
	CHECK(run({"export", "books"}) == 2);
	CHECK(run({"export", "anime", "manga"}) == 2);
	CHECK(run({"export", "--format=yaml", "anime"}) == 2);
	CHECK(err.find("error: unknown format 'yaml'") == 0);
	CHECK(has(err, "; choose xml, csv or json\nUsage: catalist export "));
	CHECK(run({"export", "--format=jsno", "anime"}) == 2);
	CHECK(err.find("error: unknown format 'jsno' (did you mean 'json'?); choose xml, csv or "
	               "json\n") == 0);
	CHECK(out.empty());
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
	// The other formats leave nothing out.
	CHECK(run({"export", "--format=csv", "anime"}) == 0);
	CHECK(err.empty());
	CHECK(std::count(out.begin(), out.end(), '\n') == 3);
}

TEST_CASE_FIXTURE(cliFixture, "cli export of an empty catalogue is an empty list") {
	CHECK(run({"export", "manga"}) == 0);
	CHECK(err.empty());
	CHECK(out.find("<user_export_type>2</user_export_type>") != std::string::npos);
	CHECK(catalist::malXml::parse(out).empty());
	CHECK(run({"export", "--format=csv", "manga"}) == 0);
	CHECK(out ==
	      "ref,type,title,status,score,progress,total,volumes,rewatched,started,finished,"
	      "updated,new\n");
	CHECK(run({"export", "--format=json", "manga"}) == 0);
	CHECK(out.empty());
	CHECK_FALSE(std::filesystem::exists("data"));
}

TEST_CASE_FIXTURE(cliFixture, "cli completion prints the script of the shell named") {
	CHECK(run({"completion", "zsh"}) == 0);
	CHECK(out.rfind("#compdef catalist cl\n", 0) == 0);
	CHECK(has(out, "'1:command:_catalist_commands'"));
	CHECK(err.empty());
	CHECK(run({"completion", "fish"}) == 0);
	CHECK(has(out, "complete -c catalist -f\n"));
	CHECK(has(out, "-a 'ls' -d 'List your titles'\n"));
	// The scripts are the program's own: they make no lists.
	CHECK_FALSE(std::filesystem::exists("data"));

	for (std::vector<std::string> wrong : {std::vector<std::string>{"completion"},
	                                       {"completion", "bash"},
	                                       {"completion", "zsh", "fish"}}) {
		CHECK(run(wrong) == 2);
		CHECK(out.empty());
		CHECK(has(err, "error: say which shell: zsh or fish\n"));
	}
}

TEST_CASE_FIXTURE(cliFixture, "cli __complete prints the refs and titles for the scripts") {
	seed();
	CHECK(run({"__complete", "refs"}) == 0);
	CHECK(out == "a:1\tSousou no Frieren\na:2\tOdd Taxi\nm:1\tBerserk\nm:2\tSousou no Frieren\n");
	CHECK(err.empty());
	CHECK(run({"__complete", "titles"}) == 0);
	CHECK(out == "Sousou no Frieren\ta:1\nOdd Taxi\ta:2\nBerserk\tm:1\nSousou no Frieren\tm:2\n");
	CHECK(run({"__complete", "statuses"}) == 0);
	CHECK(out == "watching\ncompleted\non-hold\ndropped\nplanned\n");
	CHECK(run({"__complete", "fields"}) == 0);
	CHECK(out.rfind("total=\nprogress=\nstatus=\nscore=\n", 0) == 0);
	CHECK(has(out, "start=\nfinish=\n"));
	// It is not one of the commands the person is shown.
	CHECK(run({"help"}) == 0);
	CHECK_FALSE(has(out, "__complete"));
	CHECK_FALSE(has(out, "__man"));
	CHECK(run({"completion", "zsh"}) == 0);
	CHECK_FALSE(has(out, "__complete)"));
}

TEST_CASE_FIXTURE(cliFixture, "cli __complete says nothing and exits 0 whatever goes wrong") {
	// No lists yet: nothing is made, and the old files are not imported.
	put("data/anime.csv", "id,mal_id,type,title\n1,52991,anime,Sousou no Frieren\n");
	for (const char* what : {"refs", "titles"}) {
		CHECK(run({"__complete", what}) == 0);
		CHECK(out.empty());
		CHECK(err.empty());
	}
	CHECK_FALSE(std::filesystem::exists("data/catalist.db"));
	// Lists it cannot read.
	put("data/catalist.db", "not a database");
	CHECK(run({"__complete", "refs"}) == 0);
	CHECK(out.empty());
	CHECK(err.empty());
	// A word, an option or an operand it does not know.
	for (std::vector<std::string> wrong : {std::vector<std::string>{"__complete"},
	                                       {"__complete", "nope"},
	                                       {"__complete", "--nope"},
	                                       {"__complete", "-x", "refs"}}) {
		CHECK(run(wrong) == 0);
		CHECK(out.empty());
		CHECK(err.empty());
	}
}

TEST_CASE_FIXTURE(cliFixture, "cli __man fills in the version and the commands") {
	CHECK(run({"__man"},
	          ".TH CATALIST 1 \"\" \"v@PROJECT_VERSION@\"\n.SH COMMANDS\n@COMMANDS@\n"
	          ".SH FILES\n") == 0);
	CHECK(out.rfind(std::string(".TH CATALIST 1 \"\" \"v") + catalist::kVersionString +
	                    "\"\n.SH COMMANDS\n.TP\n\\fBls\\fR ",
	                0) == 0);
	CHECK(has(out, "\n.TP\n\\fBcompletion\\fR \\fBzsh\\fR|\\fBfish\\fR\n"));
	CHECK(out.rfind("\n.SH FILES\n") + 11 == out.size());
	CHECK_FALSE(has(out, "__complete"));
	CHECK(err.empty());
}

TEST_CASE_FIXTURE(cliFixture, "cli takes every value the scripts offer") {
	seed();
	for (const char* key : {"new", "updated", "score", "progress", "date", "status", "title"}) {
		CAPTURE(key);
		CHECK(run({"ls", std::string("--sort=") + key}) == 0);
	}
	for (const char* status : {"watching", "completed", "on-hold", "dropped", "planned"}) {
		CAPTURE(status);
		CHECK(run({"ls", "-s", status}) == 0);
	}
	for (const char* format : {"xml", "csv", "json"}) {
		CAPTURE(format);
		CHECK(run({"export", std::string("--format=") + format, "anime"}) == 0);
	}
}

TEST_CASE("cli noTerminalNote says how to use the program without a terminal") {
	CHECK(catalist::noTerminalNote("cl") ==
	      "error: the tracker needs a terminal; give a command to work without one\n"
	      "Usage: cl [COMMAND] [ARG...]\nRun 'cl help' to see the commands.\n");
}
