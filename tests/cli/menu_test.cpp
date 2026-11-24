#include <doctest/doctest.h>

#include <filesystem>

#include "support/cli_fixture.hpp"

using catalist::MediaType;
using catalist::WatchStatus;
using catalist::test::CliFixture;
using catalist::test::Has;

TEST_CASE_FIXTURE(CliFixture, "menu opens at a terminal and exits with 0") {
	Seed();
	CHECK(Run({}, "0\n") == 0);
	CHECK(Has(out, "MAIN MENU"));
	CHECK(Has(out, "   1  Add a title\n"));
	CHECK(Has(out, "   0  Exit\n"));
	CHECK(Has(out, "CONTINUE\n"));
	CHECK(Has(out, "2 anime, 2 manga, 2 in progress"));
	CHECK(Has(out, "Goodbye.\n"));
}

TEST_CASE_FIXTURE(CliFixture, "menu does not open without someone to answer it") {
	Seed();
	CHECK(Run({}) == 2);
	CHECK(Has(err, "no command given"));
	CHECK(Run({}, "", 100) == 0);
	CHECK_FALSE(Has(out, "MAIN MENU"));
}

TEST_CASE_FIXTURE(CliFixture, "menu asks again after a choice that is not on it") {
	Seed();
	CHECK(Run({}, "9\nabc\n-1\n\n0\n") == 0);
	CHECK(Has(out, "Enter a number from 0 to 8.\n"));
	CHECK(Has(out, "Goodbye.\n"));
}

TEST_CASE_FIXTURE(CliFixture, "menu ends when the input ends, wherever it is") {
	Seed();
	for (const char* typed : {"2\n", "4\n", "4\ntaxi\n", "4\ntaxi\n3\n", "7\n", "8\n", "1\n", "5\n"}) {
		CAPTURE(typed);
		CHECK(Run({}, typed) == 0);
		CHECK(Has(out, "Goodbye.\n"));
	}
	CHECK(SavedTitle(MediaType::kAnime, "Odd Taxi").progress == 5);
}

TEST_CASE_FIXTURE(CliFixture, "menu shows the lists") {
	Seed();
	CHECK(Run({}, "2\n3\n\n0\n0\n") == 0);
	CHECK(Has(out, "VIEW MY LISTS"));
	CHECK(Has(out, "a:2\tanime\tOdd Taxi"));
	CHECK(Run({}, "2\n5\n3\n\n0\n0\n") == 0);
	CHECK(Has(out, "a:1\tanime\tSousou no Frieren\tcompleted"));
	CHECK_FALSE(Has(out, "a:2\tanime"));
	CHECK(Run({}, "2\n6\ntaxi odd\n\n0\n0\n") == 0);
	CHECK(Has(out, "a:2\tanime\tOdd Taxi"));
	CHECK(Run({}, "2\n7\ntaxi\n\n0\n0\n") == 0);
	CHECK(Has(out, "44265"));
}

TEST_CASE_FIXTURE(CliFixture, "menu adds a title with the status chosen") {
	KnowTitle("Dandadan", 57334);
	CHECK(Run({}, "1\nDandadan\n2\n\n0\n") == 0);
	auto added = SavedTitle(MediaType::kAnime, "Dandadan");
	CHECK(added.status == WatchStatus::kWatching);
	CHECK(Steps()[0].label == "add --status=watching Dandadan");
	CHECK(Run({}, "1\n\n0\n") == 0);
	CHECK(Run({}, "1\nSomething\n0\n0\n") == 0);
	CHECK(Saved(MediaType::kAnime).size() == 1);
}

TEST_CASE_FIXTURE(CliFixture, "menu counts the next episode of a title in progress") {
	Seed();
	CHECK(Run({}, "3\n2\n\n0\n") == 0);
	CHECK(SavedTitle(MediaType::kAnime, "Odd Taxi").progress == 6);
	CHECK(Steps()[0].label == "+ Odd Taxi");
	CHECK(Run({}, "3\n0\n0\n") == 0);
	CHECK(SavedTitle(MediaType::kAnime, "Odd Taxi").progress == 6);
}

TEST_CASE_FIXTURE(CliFixture, "menu edits one title through its own menu") {
	Seed();
	CHECK(Run({}, "4\ntaxi\n1\n\n4\n9\n\n3\n10\n\n6\n20\n\n8\n2026-01-02\n\n10\nOdd Cab\n\n0\n0\n") == 0);
	auto taxi = SavedTitle(MediaType::kAnime, "Odd Cab");
	CHECK(taxi.progress == 10);
	CHECK(taxi.score == 9);
	CHECK(taxi.total_units == 20);
	CHECK(catalist::FormatDate(taxi.start_date) == "2026-01-02");
	CHECK(taxi.title == "Odd Taxi");
	CHECK(Steps()[0].label == "set Odd Taxi title=Odd Cab");
}

TEST_CASE_FIXTURE(CliFixture, "menu refuses bad values and keeps going") {
	Seed();
	CHECK(Run({}, "4\ntaxi\n4\n11\n\n3\nlots\n\n8\n2026-13-40\n\n7\n\n0\n0\n") == 0);
	CHECK(Has(out, "Enter a number from 0 to 10.\n"));
	CHECK(Has(out, "Enter a whole number.\n"));
	CHECK(Has(out, "Enter a real date such as 2026-10-20.\n"));
	CHECK(Has(out, "Only manga have volumes.\n"));
	CHECK(Steps().empty());
}

TEST_CASE_FIXTURE(CliFixture, "menu keeps working after a command changes nothing or is refused") {
	Seed();
	CHECK(Run({}, "4\na:1\n5\n2\n\n1\n\n4\n3\n\n0\n0\n") == 0);
	CHECK(Has(err, "is completed; to start it again"));
	CHECK(Has(err, "Nothing changed.\n"));
	CHECK(SavedTitle(MediaType::kAnime, "Sousou no Frieren").score == 3);
}

TEST_CASE_FIXTURE(CliFixture, "menu finds a title from a typo or a list of matches") {
	Seed();
	CHECK(Run({}, "4\ntaxo\ny\n4\n2\n\n0\n0\n") == 0);
	CHECK(SavedTitle(MediaType::kAnime, "Odd Taxi").score == 2);
	CHECK(Run({}, "4\nnaruto\n\n0\n") == 0);
	CHECK(Has(out, "Nothing in your lists matches 'naruto'.\n"));
	CHECK(Run({}, "4\nfrieren\n1\n4\n7\n\n0\n0\n") == 0);
	CHECK(Has(err, "anime") );
	CHECK(Run({}, "4\nfrieren\n0\n\n0\n") == 0);
}

TEST_CASE_FIXTURE(CliFixture, "menu removes a title only when confirmed") {
	Seed();
	CHECK(Run({}, "5\ntaxi\nn\n\n0\n") == 0);
	CHECK(Saved(MediaType::kAnime).size() == 2);
	CHECK(Run({}, "5\ntaxi\ny\n\n0\n") == 0);
	CHECK(Saved(MediaType::kAnime).size() == 1);
	CHECK(Run({}, "7\n1\n\n0\n0\n") == 0);
	CHECK(Saved(MediaType::kAnime).size() == 2);
	CHECK(Run({}, "7\n2\n\n3\n\n0\n0\n") == 0);
	CHECK(Saved(MediaType::kAnime).size() == 1);
	CHECK(Has(out, "rm Odd Taxi"));
}

TEST_CASE_FIXTURE(CliFixture, "menu can force an undo that a later edit blocks") {
	Seed();
	REQUIRE(Run({"+", "-q", "taxi"}) == 0);
	{
		catalist::Catalogue db("data");
		REQUIRE(db.LoadAll().value());
		auto taxi = *db.GetById(MediaType::kAnime, 2);
		taxi.progress = 9;
		REQUIRE(db.UpdateEntry(taxi));
		REQUIRE(db.SaveAll());
	}
	CHECK(Run({}, "7\n1\nn\n\n0\n0\n") == 0);
	CHECK(SavedTitle(MediaType::kAnime, "Odd Taxi").progress == 9);
	CHECK(Run({}, "7\n1\ny\n\n0\n0\n") == 0);
	CHECK(SavedTitle(MediaType::kAnime, "Odd Taxi").progress == 5);
}

TEST_CASE_FIXTURE(CliFixture, "menu shows the numbers of the lists") {
	Seed();
	CHECK(Run({}, "6\n\n0\n") == 0);
	CHECK(Has(out, "anime\t2\t"));
}

TEST_CASE_FIXTURE(CliFixture, "menu exports to a file and imports it") {
	Seed();
	CHECK(Run({}, "8\n2\nanime.xml\n\n0\n0\n") == 0);
	CHECK(Has(out, "Saved anime.xml.\n"));
	CHECK(Has(catalist::test::ReadText("anime.xml"), "Odd Taxi"));

	Put("anime.xml", "keep me");
	CHECK(Run({}, "8\n2\nanime.xml\nn\n\n0\n0\n") == 0);
	CHECK(catalist::test::ReadText("anime.xml") == "keep me");
	CHECK(Run({}, "8\n2\nanime.xml\ny\n\n0\n0\n") == 0);
	CHECK(Has(catalist::test::ReadText("anime.xml"), "Odd Taxi"));

	std::filesystem::remove_all("data");
	CHECK(Run({}, "8\n1\nanime.xml\n\n0\n0\n") == 0);
	CHECK(Saved(MediaType::kAnime).size() == 2);
}
