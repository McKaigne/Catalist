#include <doctest/doctest.h>

#include <algorithm>
#include <filesystem>
#include <string>

#include "support/cli_fixture.hpp"

using catalist::test::CliFixture;
using catalist::test::Has;
using catalist::test::MakeEntry;

TEST_CASE_FIXTURE(CliFixture, "cli ls prints tab-separated lines into a pipe") {
	Seed();
	CHECK(Run({"ls"}) == 0);
	// In title order; nothing but the titles, and every field holds something.
	CHECK(out ==
	      "m:1\tmanga\tBerserk\twatching\t0\t364\t0\t0\t0\t2019-05-01\t-\t-\t0\n"
	      "a:2\tanime\tOdd Taxi\twatching\t8\t5\t13\t0\t0\t2026-10-01\t-\t-\t8\n"
	      "a:1\tanime\tSousou no Frieren\tcompleted\t10\t28\t28\t0\t0\t-\t-\t-\t0\n"
	      "m:2\tmanga\tSousou no Frieren\tplanned\t0\t0\t0\t0\t0\t-\t-\t-\t0\n");
	CHECK(err.empty());

	CHECK(Run({"ls", "--header", "taxi"}) == 0);
	CHECK(out ==
	      "ref\ttype\ttitle\tstatus\tscore\tprogress\ttotal\tvolumes\trewatched\tstarted\t"
	      "finished\tupdated\tnew\n"
	      "a:2\tanime\tOdd Taxi\twatching\t8\t5\t13\t0\t0\t2026-10-01\t-\t-\t8\n");
	// Nothing listed: nothing printed, and that is not a failure.
	CHECK(Run({"ls", "nothing like it"}) == 0);
	CHECK(out.empty());
	CHECK(err.empty());
}

TEST_CASE_FIXTURE(CliFixture, "cli ls keeps to a list, a status or a part of the title") {
	Seed();
	CHECK(Run({"ls", "-q"}) == 0);
	CHECK(out == "m:1\na:2\na:1\nm:2\n");
	CHECK(Run({"ls", "-qm"}) == 0);
	CHECK(out == "m:1\nm:2\n");
	CHECK(Run({"ls", "-q", "-a"}) == 0);
	CHECK(out == "a:2\na:1\n");
	CHECK(Run({"ls", "-qam"}) == 0);
	CHECK(out == "m:1\na:2\na:1\nm:2\n");
	CHECK(Run({"ls", "-q", "FRIEREN"}) == 0);
	CHECK(out == "a:1\nm:2\n");
	CHECK(Run({"ls", "-q", "no fri"}) == 0);
	CHECK(out == "a:1\nm:2\n");
	CHECK(Run({"ls", "-q", "-s", "watching"}) == 0);
	CHECK(out == "m:1\na:2\n");
	CHECK(Run({"ls", "-qas", "reading"}) == 0);
	CHECK(out == "a:2\n");
	CHECK(Run({"ls", "-q", "-splanned", "sousou"}) == 0);
	CHECK(out == "m:2\n");
	CHECK(Run({"ls", "-q", "-s", "on-hold"}) == 0);
	CHECK(out.empty());
	// After --, a title may start with a dash.
	CHECK(Run({"ls", "-q", "--", "-s"}) == 0);
	CHECK(out.empty());

	// Several statuses, with commas.
	CHECK(Run({"ls", "-q", "-s", "completed,planned"}) == 0);
	CHECK(out == "a:1\nm:2\n");
	CHECK(Run({"ls", "-q", "-a", "-s", "watching,on-hold"}) == 0);
	CHECK(out == "a:2\n");
	CHECK(Run({"ls", "-s", "watching,bored"}) == 2);
	CHECK(err.find("error: unknown status 'bored'\n") == 0);
	CHECK(Run({"ls", "-s", ""}) == 2);
	CHECK(err.find("error: -s needs a status\n") == 0);
	CHECK(Run({"ls", "-s", "bored"}) == 2);
	CHECK(err.find("error: unknown status 'bored'\n") == 0);
	// The words of a query need no quotes.
	CHECK(Run({"ls", "-q", "odd", "taxi"}) == 0);
	CHECK(out == "a:2\n");
	CHECK(Run({"ls", "-q", "sousou", "frieren"}) == 0);
	CHECK(out.empty());
	// Options come first.
	CHECK(Run({"ls", "taxi", "-q"}) == 2);
	CHECK(err.find("error: '-q' comes after the text; options go before it\n") == 0);
	CHECK(out.empty());
}

TEST_CASE_FIXTURE(CliFixture, "cli ls sorts by title, or by the key asked for") {
	Seed();
	CHECK(Run({"ls", "-q", "--sort=title"}) == 0);
	CHECK(out == "m:1\na:2\na:1\nm:2\n");
	CHECK(Run({"ls", "-qr"}) == 0);
	// Two titles that sort alike keep their order: anime, then manga.
	CHECK(out == "a:1\nm:2\na:2\nm:1\n");
	CHECK(Run({"ls", "-q", "--sort=score"}) == 0);
	CHECK(out.find("a:1\na:2\n") == 0);
	CHECK(Run({"ls", "-q", "--sort", "progress"}) == 0);
	CHECK(out.find("m:1\na:1\na:2\n") == 0);
	CHECK(Run({"ls", "-qr", "--sort=progress"}) == 0);
	CHECK(out == "m:2\na:2\na:1\nm:1\n");
	CHECK(Run({"ls", "-q", "--sort=new"}) == 0);
	CHECK(out.find("a:2\n") == 0);
	for (const char* key : {"updated", "date", "status"})
		CHECK(Run({"ls", "-q", std::string("--sort=") + key}) == 0);

	CHECK(Run({"ls", "--sort=colour"}) == 2);
	CHECK(err.find("error: unknown sort key 'colour'\n") == 0);
	CHECK(Run({"ls", "--sort=Title"}) == 2);
}

TEST_CASE_FIXTURE(CliFixture, "cli ls --json prints one object a line") {
	Seed();
	CHECK(Run({"ls", "--json", "taxi"}) == 0);
	CHECK(out ==
	      "{\"ref\":\"a:2\",\"type\":\"anime\",\"title\":\"Odd Taxi\",\"status\":\"watching\","
	      "\"score\":8,\"progress\":5,\"total\":13,\"volumes\":0,\"rewatched\":0,"
	      "\"started\":\"2026-10-01\",\"finished\":null,\"updated\":null,\"new\":8}\n");
	// A total that is not known is null; --header is for the tabbed form.
	CHECK(Run({"ls", "--json", "--header", "berserk"}) == 0);
	CHECK(out.find("{\"ref\":\"m:1\",\"type\":\"manga\",\"title\":\"Berserk\"") == 0);
	CHECK(Has(out, "\"progress\":364,\"total\":null,"));
	CHECK(Run({"ls", "--json"}) == 0);
	CHECK(std::count(out.begin(), out.end(), '\n') == 4);
	// The same at a terminal, and -q wins over it.
	CHECK(Run({"ls", "--json", "taxi"}, "", 100) == 0);
	CHECK(out.find("{\"ref\":\"a:2\"") == 0);
	CHECK(Run({"ls", "--json", "-q", "taxi"}) == 0);
	CHECK(out == "a:2\n");
}

TEST_CASE_FIXTURE(CliFixture, "cli ls keeps a tab or a quote in a title from breaking a line") {
	{
		catalist::Catalogue db("data");
		db.AddEntry(MakeEntry().Title("Tab\there, \"quoted\"").StartDate("2026-01-02"));
		REQUIRE(db.SaveAll());
	}
	CHECK(Run({"ls"}) == 0);
	CHECK(out ==
	      "a:1\tanime\tTab here, \"quoted\"\twatching\t0\t0\t0\t0\t0\t2026-01-02\t-\t-\t0\n");
	CHECK(Run({"ls", "--json"}) == 0);
	CHECK(Has(out, "\"title\":\"Tab\\there, \\\"quoted\\\"\","));
	CHECK(Has(out, "\"started\":\"2026-01-02\",\"finished\":null,"));
}

TEST_CASE_FIXTURE(CliFixture, "cli ls draws a table at a terminal") {
	Seed();
	// Scores are five stars, a half star a point: full, half and empty.
	const std::string eight = "\U000F04CE\U000F04CE\U000F04CE\U000F04CE\U000F04D2";
	const std::string ten = "\U000F04CE\U000F04CE\U000F04CE\U000F04CE\U000F04CE";
	CHECK(Run({"ls"}, "", 100) == 0);
	CHECK(out ==
	      "REF  TITLE              STATUS        SCORE  PROGRESS  NEW\n"
	      "m:1  Berserk            Reading       -      364/?\n"
	      "a:2  Odd Taxi           Watching      " +
	          eight +
	          "  5/13      +8\n"
	          "a:1  Sousou no Frieren  Completed     " +
	          ten +
	          "  28/28\n"
	          "m:2  Sousou no Frieren  Plan to Read  -      0/?\n");
	CHECK(err.empty());

	// A narrow terminal cuts the titles, not the other columns.
	CHECK(Run({"ls", "-a"}, "", 40) == 0);
	CHECK(out ==
	      "REF  TITLE       STATUS     SCORE  PROGRESS  NEW\n"
	      "a:2  Odd Taxi    Watching   " +
	          eight +
	          "  5/13      +8\n"
	          "a:1  Sousou no…  Completed  " +
	          ten + "  28/28\n");

	CHECK(Run({"ls", "nothing like it"}, "", 100) == 0);
	CHECK(out.empty());
	CHECK(err == "No titles to list.\n");
	CHECK(Run({"ls", "-q", "taxi"}, "", 100) == 0);
	CHECK(out == "a:2\n");

	color = true;
	CHECK(Run({"ls", "taxi"}, "", 100) == 0);
	CHECK(Has(out, "\x1b[2mREF\x1b[0m  "));
	CHECK(Has(out, "\x1b[36ma:2\x1b[0m  Odd Taxi  \x1b[32mWatching\x1b[0m  "));
	CHECK(Has(out, "  \x1b[32m+8\x1b[0m\n"));
}

TEST_CASE_FIXTURE(CliFixture, "cli show prints a title's line into a pipe") {
	Seed();
	CHECK(Run({"show", "taxi"}) == 0);
	CHECK(
	    out ==
	    "a:2\tanime\tOdd Taxi\twatching\t8\t5\t13\t0\t0\t2026-10-01\t-\t-\t8\t0\t13\t128547\t44265\t"
	    "https://anilist.co/anime/128547\t0\n");
	CHECK(err.empty());
	CHECK(Run({"show", "--header", "m:1"}) == 0);
	CHECK(out ==
	      "ref\ttype\ttitle\tstatus\tscore\tprogress\ttotal\tvolumes\trewatched\tstarted\t"
	      "finished\tupdated\tnew\ttotal_volumes\treleased\tanilist_id\tmal_id\turl\tkitsu_id\n"
	      "m:1\tmanga\tBerserk\twatching\t0\t364\t0\t0\t0\t2019-05-01\t-\t-\t0\t0\t0\t30002\t2\t"
	      "https://anilist.co/manga/30002\t0\n");
	CHECK(Run({"show", "--json", "m:1"}) == 0);
	CHECK(out ==
	      "{\"ref\":\"m:1\",\"type\":\"manga\",\"title\":\"Berserk\",\"status\":\"watching\","
	      "\"score\":0,\"progress\":364,\"total\":null,\"volumes\":0,\"rewatched\":0,"
	      "\"started\":\"2019-05-01\",\"finished\":null,\"updated\":null,\"new\":0,"
	      "\"total_volumes\":null,\"released\":null,\"anilist_id\":30002,\"mal_id\":2,"
	      "\"url\":\"https://anilist.co/manga/30002\",\"kitsu_id\":null}\n");
}

TEST_CASE_FIXTURE(CliFixture, "cli show takes several titles, by ref, by title or from a pipe") {
	Seed();
	// Each once, in the order named.
	CHECK(Run({"show", "a:1", "taxi", "a:1", "ODD"}) == 0);
	CHECK(out.find("a:1\t") == 0);
	CHECK(Has(out, "\na:2\t"));
	CHECK(std::count(out.begin(), out.end(), '\n') == 2);
	CHECK(Run({"show", "-"}, "a:2\n\nberserk\n") == 0);
	CHECK(out.find("a:2\t") == 0);
	CHECK(Has(out, "\nm:1\t"));
	CHECK(Run({"show", "m:2", "-"}, "a:2\n") == 0);
	CHECK(out.find("m:2\t") == 0);
	CHECK(Has(out, "\na:2\t"));
	// Nothing in the pipe is nothing to do.
	CHECK(Run({"show", "-"}) == 0);
	CHECK(out.empty());
	CHECK(err == "No titles named on standard input: nothing to do.\n");
	// A title that starts with a dash goes after --.
	CHECK(Run({"show", "--", "-s"}) == 1);
	CHECK(Has(err, "error: nothing in your lists matches '-s'\n"));

	CHECK(Run({"show"}) == 2);
	CHECK(err.find("error: name a title: a ref such as a:12, or part of it\n") == 0);
	CHECK(Run({"show", "taxi", ""}) == 2);
	CHECK(err.find("error: a REF cannot be empty\n") == 0);
	CHECK(out.empty());
}

TEST_CASE_FIXTURE(CliFixture, "cli show draws a card at a terminal") {
	Seed();
	CHECK(Run({"show", "taxi"}, "", 100) == 0);
	CHECK(out ==
	      "Odd Taxi\n"
	      "  Ref          a:2\n"
	      "  Type         Anime\n"
	      "  AniList      128547\n"
	      "  MyAnimeList  44265\n"
	      "  Kitsu        -\n"
	      "  Page         https://anilist.co/anime/128547\n"
	      "  Status       Watching\n"
	      "  Score        \U000F04CE\U000F04CE\U000F04CE\U000F04CE\U000F04D2  8/10\n"
	      "  Progress     5/13 episodes  (+8 new)\n"
	      "  Rewatched    0\n"
	      "  Started      2026-10-01\n"
	      "  Finished     -\n"
	      "  Updated      -\n");
	CHECK(Run({"show", "m:2", "berserk"}, "", 100) == 0);
	CHECK(out.find("Sousou no Frieren\n  Ref          m:2\n  Type         Manga\n") == 0);
	CHECK(Has(out, "  Status       Plan to Read\n  Score        -\n"));
	CHECK(Has(out, "  Progress     0/? chapters\n  Volumes      0/?\n  Reread       0\n"));
	// A blank line between two cards.
	CHECK(Has(out, "  Updated      -\n\nBerserk\n"));
	CHECK(Run({"show", "--json", "taxi"}, "", 100) == 0);
	CHECK(out.find("{\"ref\":\"a:2\"") == 0);
}

TEST_CASE_FIXTURE(CliFixture, "cli stats prints one line of numbers a list") {
	Seed();
	CHECK(Run({"stats"}) == 0);
	CHECK(out ==
	      "anime\t2\t1\t1\t0\t0\t0\t9.00\t33\t0\t0\t0.6\t0\n"
	      "manga\t2\t1\t0\t0\t0\t1\t0\t364\t0\t0\t1.3\t0\n");
	CHECK(err.empty());
	CHECK(Run({"stats", "-m", "--header"}) == 0);
	CHECK(out ==
	      "type\ttotal\twatching\tcompleted\ton_hold\tdropped\tplanned\tmean\tunits\tvolumes\t"
	      "rewatched\tdays\trewatching\n"
	      "manga\t2\t1\t0\t0\t0\t1\t0\t364\t0\t0\t1.3\t0\n");
	CHECK(Run({"stats", "-a", "--json"}, "", 100) == 0);
	CHECK(out ==
	      "{\"type\":\"anime\",\"total\":2,\"watching\":1,\"completed\":1,\"on_hold\":0,"
	      "\"dropped\":0,\"planned\":0,\"mean\":9.00,\"units\":33,\"volumes\":0,"
	      "\"rewatched\":0,\"days\":0.6,\"rewatching\":0}\n");
	// No score given yet: there is no mean.
	CHECK(Run({"stats", "-m", "--json"}) == 0);
	CHECK(Has(out, "\"planned\":1,\"mean\":null,\"units\":364,"));
	CHECK(Run({"stats", "anime"}) == 2);
	CHECK(err.find("error: stats takes no operands\nUsage: catalist stats ") == 0);
}

TEST_CASE_FIXTURE(CliFixture, "cli stats draws a card a list at a terminal") {
	Seed();
	CHECK(Run({"stats"}, "", 100) == 0);
	CHECK(out ==
	      "Anime\n"
	      "  Titles         2\n"
	      "  Watching       1\n"
	      "  Completed      1\n"
	      "  On-Hold        0\n"
	      "  Dropped        0\n"
	      "  Plan to Watch  0\n"
	      "  Rewatching     0\n"
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
	      "  Rereading      0\n"
	      "  Mean score     -\n"
	      "  Chapters       364\n"
	      "  Volumes        0\n"
	      "  Reread         0\n"
	      "  Days spent     1.3\n");
	CHECK(Run({"stats", "-m"}, "", 100) == 0);
	CHECK(out.find("Manga\n") == 0);
}

TEST_CASE_FIXTURE(CliFixture, "cli stats of an empty catalogue is all zeros, and writes nothing") {
	CHECK(Run({"stats"}) == 0);
	CHECK(out ==
	      "anime\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0.0\t0\n"
	      "manga\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0.0\t0\n");
	CHECK_FALSE(std::filesystem::exists("data"));
}
