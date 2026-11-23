#include <doctest/doctest.h>

#include <string>

#include "support/cli_fixture.hpp"

using catalist::MediaType;
using catalist::WatchStatus;
using catalist::test::CliFixture;
using catalist::test::Has;

TEST_CASE_FIXTURE(CliFixture, "cli + counts one more and says what changed") {
	Seed();
	CHECK(Run({"+", "taxi"}) == 0);
	CHECK(out == "a:2\tOdd Taxi\tprogress\t5\t6\n");
	CHECK(err.empty());
	CHECK(SavedTitle(MediaType::kAnime, "Odd Taxi").progress == 6);
	CHECK(web.posted.empty());

	// At a terminal the line is for reading.
	CHECK(Run({"+", "a:2"}, "", 100) == 0);
	CHECK(out == "Odd Taxi  6 → 7/13\n");
	CHECK(Run({"inc", "-n", "2", "berserk"}, "", 100) == 0);
	CHECK(out == "Berserk  364 → 366/?\n");
	color = true;
	CHECK(Run({"+", "a:2"}, "", 100) == 0);
	CHECK(out == "\x1b[1mOdd Taxi\x1b[0m  7 → 8/13\n");
	color = false;

	// The first one starts a title, the last one completes it.
	CHECK(Run({"+", "m:2"}) == 0);
	CHECK(out ==
	      "m:2\tSousou no Frieren\tprogress\t0\t1\n"
	      "m:2\tSousou no Frieren\tstatus\tplanned\twatching\n"
	      "m:2\tSousou no Frieren\tstarted\t-\t2026-10-20\n");
	CHECK(Run({"+", "-n5", "taxi"}, "", 100) == 0);
	CHECK(out ==
	      "Odd Taxi  8 → 13/13\n"
	      "Odd Taxi  status watching → completed\n"
	      "Odd Taxi  finished - → 2026-10-20\n");
	auto taxi = SavedTitle(MediaType::kAnime, "Odd Taxi");
	CHECK(taxi.status == WatchStatus::kCompleted);
	CHECK(catalist::FormatDate(taxi.updated_at) == "2026-10-20");
}

TEST_CASE_FIXTURE(CliFixture, "cli rewatch watches a completed title again") {
	Seed();
	// Sousou no Frieren (a:1) is completed, 28 of 28.
	CHECK(Run({"rewatch", "a:1"}) == 0);
	CHECK(out ==
	      "a:1\tSousou no Frieren\tprogress\t28\t0\n"
	      "a:1\tSousou no Frieren\tstatus\tcompleted\trewatching\n"
	      "a:1\tSousou no Frieren\tstarted\t-\t2026-10-20\n");
	CHECK(Run({"ls", "-q", "-s", "rewatching"}) == 0);
	CHECK(out == "a:1\n");
	// Plain catalist lists it beside what is being watched.
	CHECK(Run({}, "", 100) == 0);
	CHECK(Has(out, "Rewatching"));
	CHECK(Run({"+", "-n", "28", "a:1"}) == 0);
	CHECK(Has(out, "a:1\tSousou no Frieren\tstatus\trewatching\tcompleted\n"));
	CHECK(Has(out, "a:1\tSousou no Frieren\trewatched\t0\t1\n"));
	CHECK(Run({"set", "a:1", "status=rereading"}) == 0);
	CHECK(SavedTitle(MediaType::kAnime, "Sousou no Frieren").status == WatchStatus::kRewatching);
}

TEST_CASE_FIXTURE(CliFixture, "cli - counts one less") {
	Seed();
	CHECK(Run({"-", "taxi"}) == 0);
	CHECK(out == "a:2\tOdd Taxi\tprogress\t5\t4\n");
	CHECK(Run({"dec", "-n", "3", "taxi"}) == 0);
	CHECK(out == "a:2\tOdd Taxi\tprogress\t4\t1\n");
	// Below the total a completed title is open again.
	CHECK(Run({"-", "a:1"}) == 0);
	CHECK(Has(out, "a:1\tSousou no Frieren\tprogress\t28\t27\n"));
	CHECK(Has(out, "a:1\tSousou no Frieren\tstatus\tcompleted\twatching\n"));
	// A title named - is not one: the command's own name comes first.
	CHECK(Run({"-", "--", "-zq"}) == 1);
	CHECK(err.find("error: nothing in your lists matches '-zq'\n") == 0);
}

TEST_CASE_FIXTURE(CliFixture, "cli + takes several titles") {
	Seed();
	CHECK(Run({"+", "m:1", "a:2"}) == 0);
	CHECK(out ==
	      "m:1\tBerserk\tprogress\t364\t365\n"
	      "a:2\tOdd Taxi\tprogress\t5\t6\n");
	CHECK(err.empty());
	// One step for undo, named as the command was typed.
	auto history = Steps();
	REQUIRE(history.size() == 1);
	CHECK(history[0].label == "+ m:1 a:2");
	CHECK(history[0].stamp == "2026-10-20 21:30:00");
	CHECK(history[0].changes == 2);

	CHECK(Run({"+", "--all", "-q", "frieren,", "taxi"}) == 0);
	CHECK(out.empty());
	CHECK(err.empty());
	CHECK(SavedTitle(MediaType::kAnime, "Odd Taxi").progress == 7);
	CHECK(SavedTitle(MediaType::kManga, "Sousou no Frieren").progress == 1);
	// The same title named twice is counted once.
	CHECK(Run({"+", "taxi,", "a:2"}) == 0);
	CHECK(out == "a:2\tOdd Taxi\tprogress\t7\t8\n");
}

TEST_CASE_FIXTURE(CliFixture, "cli + changes nothing when one title cannot be found") {
	Seed();
	CHECK(Run({"+", "taxi,", "naruto"}) == 1);
	CHECK(err.find("error: nothing in your lists matches 'naruto'\n") == 0);
	CHECK(out.empty());
	CHECK(Run({"+", "taxi,", "frieren"}) == 1);
	CHECK(err.find("error: 'frieren' matches 2 titles:\n") == 0);
	CHECK(Run({"+", "frieren"}, "7\n") == 1);
	CHECK(Has(err, "Cancelled.\n"));
	CHECK(SavedTitle(MediaType::kAnime, "Odd Taxi").progress == 5);
	CHECK(Steps().empty());

	CHECK(Run({"+"}) == 2);
	CHECK(err.find("error: ") == 0);
	CHECK(Has(err, "Usage: catalist + [-amq] [--all] [-n N] [REF...]\n"));
	CHECK(Run({"+", "-n", "0", "taxi"}) == 2);
	CHECK(err.find("error: -n '0' is not a count of one or more\n") == 0);
	CHECK(Run({"+", "-n", "x", "taxi"}) == 2);
	CHECK(Run({"+", "-n"}) == 2);
	CHECK(Run({"+", "--by", "2", "taxi"}) == 2);
	CHECK(SavedTitle(MediaType::kAnime, "Odd Taxi").progress == 5);
}

TEST_CASE_FIXTURE(CliFixture, "cli refuses a status a title cannot take") {
	Seed();
	CHECK(Run({"resume", "a:1"}) == 1);
	CHECK(err == "error: Sousou no Frieren is completed; to start it again: catalist rewatch\n");
	CHECK(Run({"set", "a:1", "status=watching"}) == 1);
	CHECK(SavedTitle(MediaType::kAnime, "Sousou no Frieren").status == WatchStatus::kCompleted);
	CHECK(Run({"rewatch", "taxi"}) == 1);
	CHECK(err == "error: Odd Taxi is not completed; to carry on with it: catalist resume\n");
	CHECK(Run({"rewatch", "--all", "a:1", "taxi"}) == 1);
	CHECK(SavedTitle(MediaType::kAnime, "Sousou no Frieren").status == WatchStatus::kCompleted);
	CHECK(Steps().empty());
	// Lowering the progress in the same command reopens it first.
	CHECK(Run({"set", "a:1", "progress=3", "status=watching"}) == 0);
	CHECK(SavedTitle(MediaType::kAnime, "Sousou no Frieren").status == WatchStatus::kWatching);
}

TEST_CASE_FIXTURE(CliFixture, "cli set changes the fields of a title") {
	Seed();
	CHECK(Run({"set", "taxi", "score=9", "progress=13"}) == 0);
	CHECK(out ==
	      "a:2\tOdd Taxi\tprogress\t5\t13\n"
	      "a:2\tOdd Taxi\tstatus\twatching\tcompleted\n"
	      "a:2\tOdd Taxi\tscore\t8\t9\n"
	      "a:2\tOdd Taxi\tfinished\t-\t2026-10-20\n");
	CHECK(err.empty());
	auto taxi = SavedTitle(MediaType::kAnime, "Odd Taxi");
	CHECK(taxi.status == WatchStatus::kCompleted);
	CHECK(taxi.score == 9);
	CHECK(Steps().size() == 1);

	CHECK(Run({"set", "a:2", "title=ODDTAXI", "start=2026-09-30", "finish=", "rewatched=1"}, "",
	          100) == 0);
	CHECK(out ==
	      "ODDTAXI  rewatched 0 → 1\n"
	      "ODDTAXI  started 2026-10-01 → 2026-09-30\n"
	      "ODDTAXI  finished 2026-10-20 → -\n"
	      "ODDTAXI  title Odd Taxi → ODDTAXI\n");
	taxi = SavedTitle(MediaType::kAnime, "ODDTAXI");
	CHECK(catalist::FormatDate(taxi.start_date) == "2026-09-30");
	CHECK_FALSE(taxi.finish_date);
	CHECK(taxi.status == WatchStatus::kCompleted);

	// The total first, so the progress can reach it; a field's name in any
	// case, and the names the columns of ls have for the dates.
	CHECK(Run({"set", "-m", "berserk", "Progress=380", "total=380", "VOLUMES=42",
	           "started=2019-04-30", "finished=2026-10-19"}) == 0);
	auto berserk = SavedTitle(MediaType::kManga, "Berserk");
	CHECK(berserk.total_units == 380);
	CHECK(berserk.progress == 380);
	CHECK(berserk.volumes == 42);
	CHECK(berserk.status == WatchStatus::kCompleted);
	CHECK(catalist::FormatDate(berserk.start_date) == "2019-04-30");
	CHECK(catalist::FormatDate(berserk.finish_date) == "2026-10-19");

	// The status last, so a status that is given stands.
	CHECK(Run({"set", "a:1", "status=on-hold", "progress=3", "score=0"}) == 0);
	auto frieren = SavedTitle(MediaType::kAnime, "Sousou no Frieren");
	CHECK(frieren.status == WatchStatus::kOnHold);
	CHECK(frieren.progress == 3);
	CHECK(frieren.score == 0);
	// A value may hold an equals sign, and a title spaces.
	CHECK(Run({"set", "-q", "a:1", "title=1 + 1 = 2"}) == 0);
	CHECK(out.empty());
	CHECK(SavedTitle(MediaType::kAnime, "1 + 1 = 2").id == 1);
	// Every title of that name.
	CHECK(Run({"set", "--all", "1", "score=7"}) == 0);
	CHECK(Has(out, "a:1\t1 + 1 = 2\tscore\t0\t7\n"));
}

TEST_CASE_FIXTURE(CliFixture, "cli set checks every value before touching anything") {
	Seed();
	CHECK(Run({"set"}) == 2);
	CHECK(err.find("error: name a title") == 0);
	CHECK(Run({"set", "taxi"}) == 2);
	CHECK(err ==
	      "error: nothing to change: give a FIELD=VALUE such as score=8\n"
	      "Usage: catalist set [-amq] [--all] REF FIELD=VALUE...\n"
	      "Run 'catalist help set' for more.\n");
	CHECK(Run({"set", "taxi", "progress=6", "score=11"}) == 2);
	CHECK(err.find("error: score '11' is not 0 to 10\n") == 0);
	CHECK(Run({"set", "taxi", "progress=6", "status=bored"}) == 2);
	CHECK(err.find("error: status 'bored' is not watching, completed, on-hold, dropped, planned or "
	               "rewatching\n") == 0);
	// Nothing is guessed: a word that holds a status is not one.
	CHECK(Run({"set", "taxi", "status=unwatched"}) == 2);
	CHECK(Run({"set", "taxi", "status=4"}) == 2);
	CHECK(Run({"ls", "-s", "rewatched"}) == 2);
	CHECK(err.find("error: unknown status 'rewatched'") == 0);
	CHECK(Run({"ls", "-s", "watchng"}) == 2);
	CHECK(err.find("error: unknown status 'watchng'\n") == 0);
	CHECK(Run({"add", "--no-lookup", "--status=plan", "Some OVA"}) == 2);
	CHECK(Run({"ls", "-q", "-s", "Reading,ON-HOLD"}) == 0);
	CHECK(Run({"set", "taxi", "progress=lots"}) == 2);
	CHECK(err.find("error: progress 'lots' is not a count\n") == 0);
	CHECK(Run({"set", "taxi", "progress=-1"}) == 2);
	CHECK(Run({"set", "taxi", "progress=1234567"}) == 2);
	CHECK(Run({"set", "taxi", "start=yesterday"}) == 2);
	CHECK(err.find("error: start 'yesterday' is not a date such as 2026-10-20, today, or empty") ==
	      0);
	CHECK(Run({"set", "taxi", "finish=2026-02-30"}) == 2);
	CHECK(Run({"set", "taxi", "title="}) == 2);
	CHECK(err.find("error: title '' is not a title\n") == 0);
	CHECK(Run({"set", "taxi", "scor=9"}) == 2);
	CHECK(err.find("error: unknown field 'scor'\n") == 0);
	CHECK(Run({"set", "taxi", "colour=red"}) == 2);
	CHECK(err.find("error: unknown field 'colour'\n") == 0);
	CHECK(Run({"set", "taxi", "score=9", "nine"}) == 2);
	CHECK(err.find("error: 'nine' is not FIELD=VALUE") == 0);
	// The earlier form of the command.
	CHECK(Run({"set", "taxi", "-p", "13"}) == 2);
	CHECK(out.empty());
	auto taxi = SavedTitle(MediaType::kAnime, "Odd Taxi");
	CHECK(taxi.progress == 5);
	CHECK(taxi.score == 8);

	// The title is looked for once the values are known to be good.
	CHECK(Run({"set", "naruto", "score=9"}) == 1);
	CHECK(err.find("error: nothing in your lists matches 'naruto'\n") == 0);
	CHECK(Run({"set", "frieren", "score=9"}) == 1);
	CHECK(err.find("error: 'frieren' matches 2 titles:\n") == 0);
	CHECK(Run({"set", "frieren", "score=9"}, "2\n") == 0);
	CHECK(out == "m:2\tSousou no Frieren\tscore\t0\t9\n");
	CHECK(Steps().size() == 1);
}

TEST_CASE_FIXTURE(CliFixture, "cli done, hold, drop, plan and resume set the status") {
	Seed();
	CHECK(Run({"done", "--score=9", "taxi"}) == 0);
	CHECK(out ==
	      "a:2\tOdd Taxi\tprogress\t5\t13\n"
	      "a:2\tOdd Taxi\tstatus\twatching\tcompleted\n"
	      "a:2\tOdd Taxi\tscore\t8\t9\n"
	      "a:2\tOdd Taxi\tfinished\t-\t2026-10-20\n");
	CHECK(err.empty());
	CHECK(Run({"hold", "berserk"}, "", 100) == 0);
	CHECK(out == "Berserk  status watching → on-hold\n");
	CHECK(SavedTitle(MediaType::kManga, "Berserk").status == WatchStatus::kOnHold);
	CHECK(Run({"resume", "berserk"}) == 0);
	CHECK(out == "m:1\tBerserk\tstatus\ton-hold\twatching\n");
	CHECK(Run({"drop", "-q", "m:1"}) == 0);
	CHECK(out.empty());
	CHECK(SavedTitle(MediaType::kManga, "Berserk").status == WatchStatus::kDropped);
	CHECK(Run({"plan", "m:1", "a:2"}) == 0);
	CHECK(Has(out, "m:1\tBerserk\tstatus\tdropped\tplanned\n"));
	CHECK(Has(out, "a:2\tOdd Taxi\tstatus\tcompleted\tplanned\n"));
	CHECK(Steps().size() == 5);
	CHECK(Steps()[0].label == "plan m:1 a:2");
	CHECK(Steps()[0].changes == 2);
	// A manga of unknown length is done where it stands.
	CHECK(Run({"done", "--score", "0", "--all", "frieren"}) == 0);
	CHECK(out ==
	      "a:1\tSousou no Frieren\tscore\t10\t0\n"
	      "m:2\tSousou no Frieren\tstatus\tplanned\tcompleted\n"
	      "m:2\tSousou no Frieren\tstarted\t-\t2026-10-20\n"
	      "m:2\tSousou no Frieren\tfinished\t-\t2026-10-20\n");

	CHECK(Run({"done", "--score=11", "taxi"}) == 2);
	CHECK(err.find("error: --score '11' is not 0 to 10\n") == 0);
	CHECK(Run({"done", "--score=high", "taxi"}) == 2);
	CHECK(Run({"hold", "--score=3", "taxi"}) == 2);
	CHECK(err.find("error: unknown option '--score'\n") == 0);
	CHECK(Run({"drop"}) == 2);
	CHECK(Run({"drop", "naruto"}) == 1);
	CHECK(Run({"drop", "frieren"}) == 1);
	CHECK(Steps().size() == 6);
}

TEST_CASE_FIXTURE(CliFixture, "cli rm asks first, and wants -f where nobody can be asked") {
	Seed();
	CHECK(Run({"rm", "taxi"}) == 1);
	CHECK(err == "error: nobody to ask here: pass -f to remove Odd Taxi (a:2)\n");
	CHECK(Run({"rm", "taxi"}, "n\n") == 1);
	CHECK(err == "Remove Odd Taxi (a:2) from your lists? [y/N]: Cancelled.\n");
	CHECK(out.empty());
	CHECK(Saved(MediaType::kAnime).size() == 2);
	CHECK(Steps().empty());

	CHECK(Run({"rm", "taxi"}, "y\n", 100) == 0);
	CHECK(out == "Odd Taxi  removed\n");
	CHECK(Run({"rm", "-f", "a:1"}) == 0);
	CHECK(out == "a:1\tSousou no Frieren\tremoved\tcompleted\t-\n");
	CHECK(err.empty());
	CHECK(Saved(MediaType::kAnime).empty());
	CHECK(Saved(MediaType::kManga).size() == 2);
}

TEST_CASE_FIXTURE(CliFixture, "cli rm removes several titles as one step") {
	Seed();
	// One question for all of them.
	CHECK(Run({"rm", "--all", "frieren"}, "yes\n") == 0);
	CHECK(Has(err, "  a:1  Sousou no Frieren\n"));
	CHECK(Has(err, "  m:2  Sousou no Frieren\n"));
	CHECK(Has(err, "Remove these 2 titles from your lists? [y/N]: "));
	CHECK(Saved(MediaType::kAnime).size() == 1);
	CHECK(Saved(MediaType::kManga).size() == 1);
	REQUIRE(Steps().size() == 1);
	CHECK(Steps()[0].changes == 2);

	// In a script there is nobody to ask.
	CHECK(Run({"rm", "a:2", "m:1"}) == 1);
	CHECK(Has(err, "error: nobody to ask here: pass -f to remove these 2 titles\n"));
	CHECK(Saved(MediaType::kAnime).size() == 1);
	CHECK(Run({"rm", "-fq", "a:2", "m:1"}) == 0);
	CHECK(out.empty());
	CHECK(err.empty());
	CHECK(Saved(MediaType::kAnime).empty());
	CHECK(Saved(MediaType::kManga).empty());
}

TEST_CASE_FIXTURE(CliFixture, "cli rm removes nothing when a REF is wrong") {
	Seed();
	CHECK(Run({"rm", "-f", "taxi", "nope"}) == 1);
	CHECK(Saved(MediaType::kAnime).size() == 2);
	// Two titles are called Frieren.
	CHECK(Run({"rm", "-f", "frieren"}) == 1);
	CHECK(Saved(MediaType::kAnime).size() == 2);
	CHECK(Saved(MediaType::kManga).size() == 2);
	CHECK(Run({"rm"}) == 2);
	CHECK(Run({"rm", "-f"}) == 2);
	CHECK(out.empty());
}

TEST_CASE_FIXTURE(CliFixture, "cli rm removes only the title the person confirmed") {
	Seed();
	// The ref passes to a new title while the question is open.
	CHECK(RunAsked({"rm", "a:2"}, "y\n", [this] {
		      CHECK(Run({"rm", "-fq", "a:2"}) == 0);
		      CHECK(Run({"add", "-q", "--no-lookup", "Newcomer"}) == 0);
	      }) == 1);
	CHECK(Has(err, "error: Odd Taxi (a:2) was removed or replaced meanwhile; nothing was changed\n"));
	CHECK(SavedTitle(MediaType::kAnime, "Newcomer").id == 2);

	// New progress on the same title is no reason to refuse, and undo brings
	// it back as it was when it went.
	CHECK(RunAsked({"rm", "a:2"}, "y\n",
	               [this] { CHECK(Run({"set", "-q", "a:2", "progress=4"}) == 0); }) == 0);
	CHECK(Saved(MediaType::kAnime).size() == 1);
	CHECK(Run({"undo", "-q"}) == 0);
	CHECK(SavedTitle(MediaType::kAnime, "Newcomer").progress == 4);
}

TEST_CASE_FIXTURE(CliFixture, "cli holds no lock while the person chooses a title") {
	Seed();
	CHECK(RunAsked({"hold", "frieren"}, "1\n",
	               [this] { CHECK(Run({"+", "-q", "taxi"}) == 0); }) == 0);
	CHECK(SavedTitle(MediaType::kAnime, "Odd Taxi").progress == 6);
	CHECK(Saved(MediaType::kAnime)[0].status == WatchStatus::kOnHold);

	// What another program changed meanwhile is kept.
	CHECK(RunAsked({"set", "frieren", "score=9"}, "2\n",
	               [this] { CHECK(Run({"set", "-q", "m:2", "progress=4"}) == 0); }) == 0);
	auto manga = Saved(MediaType::kManga)[1];
	CHECK(manga.score == 9);
	CHECK(manga.progress == 4);
}

TEST_CASE_FIXTURE(CliFixture, "cli takes a lone - for no title, though titles hold hyphens") {
	Seed();
	CHECK(Run({"add", "-q", "--no-lookup", "Re-Zero"}) == 0);
	const std::string said =
	    "error: punctuation alone is not a title: give a ref such as a:12, or part of a title\n";
	for (const std::vector<std::string>& line : {std::vector<std::string>{"rm", "-f", "-"},
	                                             {"done", "-"},
	                                             {"+", "a:2", "-"},
	                                             {"show", "-"},
	                                             {"set", "-", "score=5"}}) {
		CHECK(Run(line) == 2);
		CHECK(err.find(said) == 0);
	}
	CHECK(SavedTitle(MediaType::kAnime, "Re-Zero").status == WatchStatus::kPlanToWatch);
	CHECK(Saved(MediaType::kAnime).size() == 3);
	CHECK(Steps().size() == 1);
	// Part of a title may still hold one.
	CHECK(Run({"done", "-q", "re-z"}) == 0);
	CHECK(SavedTitle(MediaType::kAnime, "Re-Zero").status == WatchStatus::kCompleted);
}

TEST_CASE_FIXTURE(CliFixture, "cli set takes today for a date") {
	Seed();
	CHECK(Run({"set", "a:1", "start=today", "finish=today"}) == 0);
	auto frieren = SavedTitle(MediaType::kAnime, "Sousou no Frieren");
	CHECK(catalist::FormatDate(frieren.start_date) == "2026-10-20");
	CHECK(catalist::FormatDate(frieren.finish_date) == "2026-10-20");
}
