#include <doctest/doctest.h>

#include <algorithm>
#include <filesystem>
#include <string>

#include "support/cliFixture.hpp"

using catalist::test::cliFixture;
using catalist::test::has;
using catalist::test::makeEntry;

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

	// Several statuses, with commas.
	CHECK(run({"ls", "-q", "-s", "completed,planned"}) == 0);
	CHECK(out == "a:1\nm:2\n");
	CHECK(run({"ls", "-q", "-a", "-s", "watching,on-hold"}) == 0);
	CHECK(out == "a:2\n");
	CHECK(run({"ls", "-s", "watching,bored"}) == 2);
	CHECK(err.find("error: unknown status 'bored'\n") == 0);
	CHECK(run({"ls", "-s", ""}) == 2);
	CHECK(err.find("error: -s needs a status\n") == 0);
	CHECK(run({"ls", "-s", "bored"}) == 2);
	CHECK(err.find("error: unknown status 'bored'\n") == 0);
	// The words of a query need no quotes.
	CHECK(run({"ls", "-q", "odd", "taxi"}) == 0);
	CHECK(out == "a:2\n");
	CHECK(run({"ls", "-q", "sousou", "frieren"}) == 0);
	CHECK(out.empty());
	// Options come first.
	CHECK(run({"ls", "taxi", "-q"}) == 2);
	CHECK(err.find("error: '-q' comes after the text; options go before it\n") == 0);
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
