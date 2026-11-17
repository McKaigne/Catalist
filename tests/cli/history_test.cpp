#include <doctest/doctest.h>

#include <filesystem>
#include <string>

#include "support/cli_fixture.hpp"

using catalist::MediaType;
using catalist::WatchStatus;
using catalist::test::CliFixture;
using catalist::test::Has;

TEST_CASE_FIXTURE(CliFixture, "cli undo takes back the last command, and redo makes it again") {
	Seed();
	CHECK(Run({"undo"}) == 0);
	CHECK(err == "Nothing to undo.\n");
	CHECK(out.empty());
	CHECK(Run({"redo"}) == 0);
	CHECK(err == "Nothing to redo.\n");

	REQUIRE(Run({"+", "taxi"}) == 0);
	REQUIRE(Run({"done", "--score=9", "taxi"}) == 0);
	CHECK(Run({"undo"}) == 0);
	CHECK(err == "Undid 'done --score=9 taxi' (2026-10-20 21:30:00).\n");
	CHECK(out ==
	      "a:2\tOdd Taxi\tprogress\t13\t6\n"
	      "a:2\tOdd Taxi\tstatus\tcompleted\twatching\n"
	      "a:2\tOdd Taxi\tscore\t9\t8\n"
	      "a:2\tOdd Taxi\tfinished\t2026-10-20\t-\n");
	auto taxi = SavedTitle(MediaType::kAnime, "Odd Taxi");
	CHECK(taxi.progress == 6);
	CHECK(taxi.status == WatchStatus::kWatching);
	CHECK_FALSE(taxi.finish_date);

	CHECK(Run({"redo"}, "", 100) == 0);
	CHECK(err == "Redid 'done --score=9 taxi' (2026-10-20 21:30:00).\n");
	CHECK(out.find("Odd Taxi  6 → 13/13\nOdd Taxi  status watching → completed\n") == 0);
	CHECK(SavedTitle(MediaType::kAnime, "Odd Taxi").score == 9);
	CHECK(Run({"redo"}) == 0);
	CHECK(err == "Nothing to redo.\n");

	// Several steps, newest first; what they did between them is printed once.
	CHECK(Run({"undo", "-n", "2"}) == 0);
	CHECK(err ==
	      "Undid 'done --score=9 taxi' (2026-10-20 21:30:00).\n"
	      "Undid '+ taxi' (2026-10-20 21:30:00).\n");
	CHECK(Has(out, "a:2\tOdd Taxi\tprogress\t13\t5\n"));
	CHECK(SavedTitle(MediaType::kAnime, "Odd Taxi").progress == 5);
	CHECK(Run({"redo", "-qn9"}) == 0);
	CHECK(out.empty());
	CHECK(err.empty());
	CHECK(SavedTitle(MediaType::kAnime, "Odd Taxi").progress == 13);

	// A new change ends what could be redone.
	REQUIRE(Run({"undo"}) == 0);
	REQUIRE(Run({"hold", "berserk"}) == 0);
	CHECK(Run({"redo"}) == 0);
	CHECK(err == "Nothing to redo.\n");

	CHECK(Run({"undo", "taxi"}) == 2);
	CHECK(err.find("error: undo takes no operands\n") == 0);
	CHECK(Run({"undo", "-n", "0"}) == 2);
	CHECK(Run({"redo", "-n", "many"}) == 2);
	CHECK(Run({"redo", "--all"}) == 2);
}

TEST_CASE_FIXTURE(CliFixture, "cli undo brings a removed title back under its ref") {
	Seed();
	REQUIRE(Run({"rm", "-f", "taxi"}) == 0);
	CHECK(Saved(MediaType::kAnime).size() == 1);
	CHECK(Run({"undo"}) == 0);
	CHECK(err == "Undid 'rm -f taxi' (2026-10-20 21:30:00).\n");
	CHECK(out == "a:2\tOdd Taxi\tadded\t-\twatching\n");
	auto taxi = SavedTitle(MediaType::kAnime, "Odd Taxi");
	CHECK(taxi.id == 2);
	CHECK(taxi.anilist_id == 128547);
	CHECK(taxi.progress == 5);
	CHECK(Run({"redo"}, "", 100) == 0);
	CHECK(out == "Odd Taxi  removed\n");
	CHECK(Saved(MediaType::kAnime).size() == 1);
	CHECK(Run({"undo"}, "", 100) == 0);
	CHECK(out == "Odd Taxi  added as watching (a:2)\n");
}

TEST_CASE_FIXTURE(CliFixture, "cli undo changes nothing when a title has changed since") {
	Seed();
	REQUIRE(Run({"+", "taxi"}) == 0);
	REQUIRE(Run({"+", "berserk"}) == 0);
	ChangeBehindTheHistory(MediaType::kAnime, "Odd Taxi", 9);

	CHECK(Run({"undo", "-n", "2"}) == 1);
	CHECK(err ==
	      "error: cannot undo '+ taxi' (2026-10-20 21:30:00): Odd Taxi (a:2) has changed since: "
	      "progress is now 9, not 6\n"
	      "Nothing was changed. With -f the step is taken all the same, where it still can "
	      "be.\n");
	CHECK(out.empty());
	CHECK(SavedTitle(MediaType::kManga, "Berserk").progress == 365);
	CHECK(SavedTitle(MediaType::kAnime, "Odd Taxi").progress == 9);
	CHECK_FALSE(Steps()[0].undone);

	CHECK(Run({"undo", "-f", "-n", "2"}) == 0);
	CHECK(Has(out, "a:2\tOdd Taxi\tprogress\t9\t5\n"));
	CHECK(SavedTitle(MediaType::kManga, "Berserk").progress == 364);
	CHECK(SavedTitle(MediaType::kAnime, "Odd Taxi").progress == 5);

	// The step undone last is the first to be made again.
	ChangeBehindTheHistory(MediaType::kAnime, "Odd Taxi", 1);
	CHECK(Run({"redo"}) == 1);
	CHECK(err.find("error: cannot redo '+ taxi' (2026-10-20 21:30:00): Odd Taxi (a:2) has "
	               "changed since: progress is now 1, not 5\n") == 0);
	CHECK(Run({"redo", "-f"}) == 0);
	CHECK(SavedTitle(MediaType::kAnime, "Odd Taxi").progress == 6);
	CHECK(SavedTitle(MediaType::kManga, "Berserk").progress == 364);
}

TEST_CASE_FIXTURE(CliFixture, "cli log lists the steps, newest first") {
	Seed();
	CHECK(Run({"log"}) == 0);
	CHECK(out.empty());
	CHECK(err.empty());
	CHECK(Run({"log"}, "", 100) == 0);
	CHECK(out.empty());
	CHECK(err == "Nothing has been changed yet.\n");

	REQUIRE(Run({"+", "--all", "frieren", "taxi"}) == 0);
	REQUIRE(Run({"set", "a:2", "score=9"}) == 0);
	REQUIRE(Run({"hold", "berserk"}) == 0);
	REQUIRE(Run({"undo"}) == 0);
	CHECK(Run({"log"}) == 0);
	CHECK(out ==
	      "3\t2026-10-20 21:30:00\tundone\t1\thold berserk\n"
	      "2\t2026-10-20 21:30:00\tdone\t1\tset a:2 score=9\n"
	      "1\t2026-10-20 21:30:00\tdone\t2\t+ --all frieren taxi\n");
	CHECK(err.empty());
	CHECK(Run({"log", "--header", "-n", "1"}) == 0);
	CHECK(out ==
	      "tx\tstamp\tstate\tchanges\tlabel\n"
	      "3\t2026-10-20 21:30:00\tundone\t1\thold berserk\n");
	CHECK(Run({"log", "--json", "-n2"}) == 0);
	CHECK(out ==
	      "{\"tx\":3,\"stamp\":\"2026-10-20 21:30:00\",\"state\":\"undone\",\"changes\":1,"
	      "\"label\":\"hold berserk\"}\n"
	      "{\"tx\":2,\"stamp\":\"2026-10-20 21:30:00\",\"state\":\"done\",\"changes\":1,"
	      "\"label\":\"set a:2 score=9\"}\n");
	CHECK(Run({"log"}, "", 100) == 0);
	CHECK(out ==
	      "TX  WHEN                 STATE   CHANGES  COMMAND\n"
	      " 3  2026-10-20 21:30:00  undone        1  hold berserk\n"
	      " 2  2026-10-20 21:30:00  done          1  set a:2 score=9\n"
	      " 1  2026-10-20 21:30:00  done          2  + --all frieren taxi\n");

	// A long command is cut to 60 characters, never inside a character.
	REQUIRE(Run({"set", "a:2", "title=" + std::string(40, 'x') + "葬送のフリーレン"}) == 0);
	CHECK(Steps()[0].label == "set a:2 title=" + std::string(40, 'x') + "葬送のフリー");

	CHECK(Run({"log", "taxi"}) == 2);
	CHECK(Run({"log", "-n", "0"}) == 2);
	// Nothing is stored yet: nothing to list, and nothing is created.
	std::filesystem::remove_all("data");
	CHECK(Run({"log"}) == 0);
	CHECK(out.empty());
	CHECK_FALSE(std::filesystem::exists("data"));
}
