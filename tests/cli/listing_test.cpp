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
	      "m:1\tmanga\tBerserk\twatching\t0\t364\t0\t0\t0\t2019-05-01\t-\t-\n"
	      "a:2\tanime\tOdd Taxi\twatching\t8\t5\t13\t0\t0\t2026-10-01\t-\t-\n"
	      "a:1\tanime\tSousou no Frieren\tcompleted\t10\t28\t28\t0\t0\t-\t-\t-\n"
	      "m:2\tmanga\tSousou no Frieren\tplanned\t0\t0\t0\t0\t0\t-\t-\t-\n");
	CHECK(err.empty());

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
	CHECK(Run({"ls", "-q", "--", "-zq"}) == 0);
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
	CHECK(Run({"ls", "-q", "frieren", "sousou"}) == 0);
	CHECK(out == "a:1\nm:2\n");
	CHECK(Run({"ls", "taxi", "-q"}) == 0);
	CHECK(out == "a:2\n");
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
	CHECK(Run({"ls", "--sort=new"}) == 2);
	for (const char* key : {"updated", "date", "status"})
		CHECK(Run({"ls", "-q", std::string("--sort=") + key}) == 0);

	CHECK(Run({"ls", "--sort=colour"}) == 2);
	CHECK(err.find("error: unknown sort key 'colour'\n") == 0);
	CHECK(Run({"ls", "--sort=Title"}) == 2);
}

TEST_CASE_FIXTURE(CliFixture, "cli ls keeps a tab or a quote in a title from breaking a line") {
	{
		catalist::Catalogue db("data");
		db.AddEntry(MakeEntry().Title("Tab\there, \"quoted\"").StartDate("2026-01-02"));
		REQUIRE(db.SaveAll());
	}
	CHECK(Run({"ls"}) == 0);
	CHECK(out == "a:1\tanime\tTab here, \"quoted\"\twatching\t0\t0\t0\t0\t0\t2026-01-02\t-\t-\n");
}

TEST_CASE_FIXTURE(CliFixture, "cli ls draws a table at a terminal") {
	Seed();
	// Scores are five stars, a half star a point: full, half and empty.
	const std::string eight = "\U000F04CE\U000F04CE\U000F04CE\U000F04CE\U000F04D2";
	const std::string ten = "\U000F04CE\U000F04CE\U000F04CE\U000F04CE\U000F04CE";
	CHECK(Run({"ls"}, "", 100) == 0);
	CHECK(out ==
	      "ANIME\n"
	      "Odd Taxi           Watching      " +
	          eight +
	          "  5/13\n"
	          "Sousou no Frieren  Completed     " +
	          ten +
	          "  28/28\n"
	          "\n"
	          "MANGA\n"
	          "Berserk            Reading       -      364/?\n"
	          "Sousou no Frieren  Plan to Read  -      0/?\n");
	CHECK(err.empty());

	// A narrow terminal cuts the titles, not the other columns.
	CHECK(Run({"ls", "-a"}, "", 40) == 0);
	CHECK(out ==
	      "ANIME\n"
	      "Odd Taxi         Watching   " +
	          eight +
	          "  5/13\n"
	          "Sousou no Frie…  Completed  " +
	          ten + "  28/28\n");

	CHECK(Run({"ls", "nothing like it"}, "", 100) == 0);
	CHECK(out.empty());
	CHECK(err == "No titles to list.\n");
	CHECK(Run({"ls", "-q", "taxi"}, "", 100) == 0);
	CHECK(out == "a:2\n");

	color = true;
	CHECK(Run({"ls", "taxi"}, "", 100) == 0);
	CHECK(Has(out, "\x1b[1mANIME\x1b[0m\n"));
	CHECK(Has(out, "Odd Taxi  \x1b[32mWatching\x1b[0m  "));
}

TEST_CASE_FIXTURE(CliFixture, "cli show prints a title's line into a pipe") {
	Seed();
	CHECK(Run({"show", "taxi"}) == 0);
	CHECK(out ==
	      "a:2\tanime\tOdd Taxi\twatching\t8\t5\t13\t0\t0\t2026-10-01\t-\t-\t0\t128547\t"
	      "44265\thttps://anilist.co/anime/128547\tOdd Taxi\n");
	CHECK(err.empty());
}

TEST_CASE_FIXTURE(CliFixture, "cli show takes several titles, by ref or by title") {
	Seed();
	// Each once, in the order named.
	CHECK(Run({"show", "a:1", "taxi,", "a:1", "ODD"}) == 0);
	CHECK(out.find("a:1\t") == 0);
	CHECK(Has(out, "\na:2\t"));
	CHECK(std::count(out.begin(), out.end(), '\n') == 2);
	// A title that starts with a dash goes after --.
	CHECK(Run({"show", "--", "-zq"}) == 1);
	CHECK(Has(err, "error: nothing in your lists matches '-zq'\n"));

	CHECK(Run({"show"}) == 2);
	CHECK(err.find("error: name a title: any part of its name\n") == 0);
	CHECK(Run({"show", "taxi", ""}) == 2);
	CHECK(err.find("error: a title cannot be empty\n") == 0);
	CHECK(out.empty());
}

TEST_CASE_FIXTURE(CliFixture, "cli show draws a card at a terminal") {
	Seed();
	CHECK(Run({"show", "taxi"}, "", 100) == 0);
	CHECK(out ==
	      "Odd Taxi\n"
	      "  Type         Anime\n"
	      "  AniList      128547\n"
	      "  MyAnimeList  44265\n"
	      "  Page         https://anilist.co/anime/128547\n"
	      "  Status       Watching\n"
	      "  Score        \U000F04CE\U000F04CE\U000F04CE\U000F04CE\U000F04D2  8/10\n"
	      "  Progress     5/13 episodes\n"
	      "  Rewatched    0\n"
	      "  Started      2026-10-01\n"
	      "  Finished     -\n"
	      "  Updated      -\n");
	CHECK(Run({"show", "m:2", "berserk"}, "", 100) == 0);
	CHECK(out.find("Sousou no Frieren\n  Type         Manga\n") == 0);
	CHECK(Has(out, "  Status       Plan to Read\n  Score        -\n"));
	CHECK(Has(out, "  Progress     0/? chapters\n  Volumes      0/?\n  Reread       0\n"));
	// A blank line between two cards.
	CHECK(Has(out, "  Updated      -\n\nBerserk\n"));
}

TEST_CASE_FIXTURE(CliFixture, "cli stats prints one line of numbers a list") {
	Seed();
	CHECK(Run({"stats"}) == 0);
	CHECK(out ==
	      "anime\t2\t1\t1\t0\t0\t0\t9.00\t33\t0\t0\t0.6\t0\n"
	      "manga\t2\t1\t0\t0\t0\t1\t0\t364\t0\t0\t1.3\t0\n");
	CHECK(err.empty());
	CHECK(Run({"stats", "-m"}) == 0);
	CHECK(out == "manga\t2\t1\t0\t0\t0\t1\t0\t364\t0\t0\t1.3\t0\n");
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

TEST_CASE_FIXTURE(CliFixture, "cli shows the English name, and romaji only on show") {
	Seed();
	{
		catalist::Catalogue db("data");
		REQUIRE(db.LoadAll().value());
		auto frieren = *db.GetById(catalist::MediaType::kAnime, 1);
		frieren.english_title = "Frieren: Beyond Journey's End";
		REQUIRE(db.UpdateEntry(frieren));
		REQUIRE(db.SaveAll());
	}
	CHECK(Run({"ls", "-a"}, "", 100) == 0);
	CHECK(Has(out, "Frieren: Beyond Journey's End"));
	CHECK_FALSE(Has(out, "Sousou"));
	CHECK(Run({"ls", "-q", "sousou", "-a"}) == 0);
	CHECK(out == "a:1\n");
	CHECK(Run({"show", "a:1"}, "", 100) == 0);
	CHECK(out.find("Frieren: Beyond Journey's End\n  Romaji       Sousou no Frieren\n") == 0);
	CHECK(Run({"show", "a:1"}) == 0);
	CHECK(out.find("a:1\tanime\tFrieren: Beyond Journey's End\t") == 0);
	CHECK(Has(out, "\tSousou no Frieren\n"));
}

TEST_CASE_FIXTURE(CliFixture, "cli home view says what to do when it is empty") {
	CHECK(Run({}, "", 100) == 0);
	CHECK(Has(err, "Nothing here yet."));
	CHECK(Has(err, "catalist import FILE"));
	Seed();
	REQUIRE(Run({"hold", "-q", "taxi,", "berserk"}) == 0);
	CHECK(Run({}, "", 100) == 0);
	CHECK(Has(err, "Nothing in progress."));
	CHECK(Has(err, "catalist resume"));
}

TEST_CASE_FIXTURE(CliFixture, "cli pages a list taller than the terminal") {
	Seed();
	std::string paged;
	pager = [&paged](const std::string& text) {
		paged = text;
		return true;
	};
	rows = 40;
	CHECK(Run({"ls"}, "", 100) == 0);
	CHECK(paged.empty());
	CHECK(Has(out, "Odd Taxi"));

	rows = 4;
	CHECK(Run({"ls"}, "", 100) == 0);
	CHECK(Has(paged, "Odd Taxi"));
	CHECK(out.empty());

	paged.clear();
	CHECK(Run({"ls"}) == 0);
	CHECK(paged.empty());
	CHECK(Has(out, "a:2\t"));

	pager = [](const std::string&) { return false; };
	CHECK(Run({"ls"}, "", 100) == 0);
	CHECK(Has(out, "Odd Taxi"));
}
