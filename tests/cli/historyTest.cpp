#include <doctest/doctest.h>

#include <filesystem>
#include <string>

#include "support/cliFixture.hpp"

using catalist::mediaType;
using catalist::watchStatus;
using catalist::test::cliFixture;
using catalist::test::has;

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
