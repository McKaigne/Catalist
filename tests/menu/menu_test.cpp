#include <doctest/doctest.h>

#include <filesystem>
#include <format>

#include "support/menu_fixture.hpp"

using catalist::MediaType;
using catalist::WatchStatus;
using catalist::test::Has;
using catalist::test::MenuFixture;

TEST_CASE_FIXTURE(MenuFixture, "menu shows the main menu and exits with 0") {
	Seed();
	CHECK(Run("0\n") == 0);
	CHECK(out.starts_with("Catalist\n"));
	CHECK(Has(out, "1. Add Title\n2. View Lists\n3. Log Progress\n4. Edit Title\n"
	               "5. Remove Title\n6. Stats\n7. Import & Export\n0. Exit\n"));
	CHECK_FALSE(Has(out, "Continue"));
	CHECK(Has(out, "Enter choice [0-7]: "));
	CHECK(Has(out, "2 anime, 2 manga, 2 in progress"));
	CHECK(Has(out, "Goodbye!\n"));
}

TEST_CASE_FIXTURE(MenuFixture, "menu says what to do when the lists are empty") {
	CHECK(Run("0\n") == 0);
	CHECK(Has(out, "Your lists are empty."));
	CHECK_FALSE(std::filesystem::exists("catalist.csv"));
}

TEST_CASE_FIXTURE(MenuFixture, "menu asks again after a choice that is not on it") {
	Seed();
	CHECK(Run("9\nabc\n-1\n\n0\n") == 0);
	CHECK(Has(out, "Enter a number from 0 to 7.\n"));
	CHECK(Has(out, "Goodbye!\n"));
}

TEST_CASE_FIXTURE(MenuFixture, "menu ends when the input ends, wherever it is") {
	Seed();
	for (const char* typed : {"", "1\n", "2\n", "3\n", "4\n", "4\ntaxi\n", "4\ntaxi\n3\n", "5\n",
	                          "6\n", "7\n", "7\n1\n"}) {
		CAPTURE(typed);
		CHECK(Run(typed) == 0);
		CHECK(Has(out, "Goodbye!\n"));
	}
	CHECK(SavedTitle(MediaType::kAnime, "Odd Taxi").progress == 5);
}

TEST_CASE_FIXTURE(MenuFixture, "menu shows the lists in sections") {
	Seed();
	CHECK(Run("2\n2\n\n0\n0\n") == 0);
	CHECK(Has(out, "Catalist / View Lists / All Titles (4)"));
	CHECK(Has(out, "Anime\n"));
	CHECK(Has(out, "Manga\n"));
	CHECK(Run("2\n3\n\n0\n0\n") == 0);
	CHECK(Has(out, "Catalist / View Lists / Anime (2)"));
	CHECK(Run("2\n5\n3\n\n0\n0\n") == 0);
	CHECK(Has(out, "Catalist / View Lists / By Status / Completed (1)"));
	CHECK(Run("2\n6\ntaxi odd\n\n0\n0\n") == 0);
	CHECK(Has(out, "Catalist / View Lists / Search Lists / taxi odd (1)"));
	CHECK(Run("2\n7\ntaxi\n\n0\n0\n") == 0);
	CHECK(Has(out, "MyAnimeList  44265"));
}

TEST_CASE_FIXTURE(MenuFixture, "menu pages a long list") {
	{
		catalist::Catalogue db("catalist.csv");
		for (int i = 1; i <= 45; ++i)
			db.AddEntry(catalist::test::MakeEntry()
			                .MalId(i)
			                .Title(std::format("Show {:02}", i))
			                .Status(WatchStatus::kPlanToWatch));
		REQUIRE(db.Save("2026-11-24 10:00:00"));
	}
	CHECK(Run("2\n2\n1\n1\n1\n2\n0\n0\n0\n") == 0);
	CHECK(Has(out, "Page 1 of 3"));
	CHECK(Has(out, "Page 3 of 3"));
	CHECK(Has(out, "Show 45"));
}

TEST_CASE_FIXTURE(MenuFixture, "menu shows the English name, and romaji only on the card") {
	Seed();
	{
		catalist::Catalogue db("catalist.csv");
		REQUIRE(db.Load().value());
		auto frieren = *db.GetById(MediaType::kAnime, 1);
		frieren.english_title = "Frieren: Beyond Journey's End";
		db.UpdateEntry(frieren);
		REQUIRE(db.Save("2026-11-24 10:00:00"));
	}
	CHECK(Run("2\n3\n\n0\n0\n") == 0);
	CHECK(Has(out, "Frieren: Beyond Journey's End"));
	CHECK_FALSE(Has(out, "Sousou"));
	CHECK(Run("2\n7\nbeyond\n\n0\n0\n") == 0);
	CHECK(Has(out, "  Romaji       Sousou no Frieren\n"));
}

TEST_CASE_FIXTURE(MenuFixture, "menu adds a title with the status chosen") {
	KnowTitle("Dandadan", 57334);
	CHECK(Run("1\n1\nDandadan\n2\n\n0\n") == 0);
	CHECK(Has(out, "Dandadan added as Watching.\n"));
	auto added = SavedTitle(MediaType::kAnime, "Dandadan");
	CHECK(added.status == WatchStatus::kWatching);
	CHECK(added.mal_id == 57334);

	CHECK(Run("1\n1\nDandadan\n\n0\n") == 0);
	CHECK(Has(out, "Dandadan is in your anime list already (Watching).\n"));
	CHECK(Saved(MediaType::kAnime).size() == 1);
}

TEST_CASE_FIXTURE(MenuFixture, "menu add looks at both types, numbered straight through") {
	web.Respond(R"("search":"berserk")", R"({"data":{
	  "anime":{"media":[{"id":33,"idMal":33,"title":{"romaji":"Kenpuu Denki Berserk",
	    "english":"Berserk"},"episodes":25,"startDate":{"year":1997}}]},
	  "manga":{"media":[{"id":30002,"idMal":2,"title":{"romaji":"Berserk"},
	    "startDate":{"year":1989}}]}}})");
	CHECK(Run("1\n1\nberserk\n2\n1\n\n0\n") == 0);
	CHECK(Has(out, "Anime\n"));
	CHECK(Has(out, "Manga\n"));
	CHECK(Saved(MediaType::kManga).size() == 1);
	CHECK(Saved(MediaType::kAnime).empty());
	CHECK(Run("1\n1\nberserk\n0\n0\n") == 0);
	CHECK(Saved(MediaType::kAnime).empty());
}

TEST_CASE_FIXTURE(MenuFixture, "menu add takes the one exact name without asking") {
	web.Respond(R"("search":"frieren beyond journeys end")", R"({"data":{"anime":{"media":[
	  {"id":154587,"idMal":52991,"title":{"romaji":"Sousou no Frieren",
	   "english":"Frieren: Beyond Journey's End"},"episodes":28},
	  {"id":182255,"idMal":59978,"title":{"romaji":"Sousou no Frieren 2nd Season"}}]},
	  "manga":{"media":[]}}})");
	CHECK(Run("1\n2\nfrieren beyond journeys end\n1\n\n0\n") == 0);
	CHECK(SavedTitle(MediaType::kAnime, "Frieren: Beyond Journey's End").mal_id == 52991);
}

TEST_CASE_FIXTURE(MenuFixture, "menu add says when AniList cannot help") {
	CHECK(Run("1\n1\ndandadan\n\n0\n") == 0);
	CHECK(Has(out, "Error: could not reach AniList"));
	web.Respond(R"("search":"zzz")", R"({"data":{"anime":{"media":[]},"manga":{"media":[]}}})");
	CHECK(Run("1\n1\nzzz\n\n0\n") == 0);
	CHECK(Has(out, "AniList has no title matching 'zzz'.\n"));
	CHECK_FALSE(std::filesystem::exists("catalist.csv"));
}

TEST_CASE_FIXTURE(MenuFixture, "menu counts the next episode of a title in progress") {
	Seed();
	CHECK(Run("3\n2\n\n0\n") == 0);
	CHECK(SavedTitle(MediaType::kAnime, "Odd Taxi").progress == 6);
	CHECK(Run("3\n0\n0\n") == 0);
	CHECK(SavedTitle(MediaType::kAnime, "Odd Taxi").progress == 6);
}

TEST_CASE_FIXTURE(MenuFixture, "menu edits one title through its own menu") {
	Seed();
	CHECK(Run("4\ntaxi\n1\n\n4\n9\n\n3\n10\n\n6\n20\n\n8\n2026-01-02\n\n10\nOdd Cab\n\n0\n0\n") ==
	      0);
	auto taxi = SavedTitle(MediaType::kAnime, "Odd Cab");
	CHECK(taxi.progress == 10);
	CHECK(taxi.score == 9);
	CHECK(taxi.total_units == 20);
	CHECK(catalist::FormatDate(taxi.start_date) == "2026-01-02");
	CHECK(taxi.title == "Odd Taxi");
}

TEST_CASE_FIXTURE(MenuFixture, "menu refuses bad values and keeps going") {
	Seed();
	std::string before = catalist::test::ReadText("catalist.csv");
	CHECK(Run("4\ntaxi\n4\n11\n\n3\nlots\n\n8\n2026-13-40\n\n7\n\n0\n0\n") == 0);
	CHECK(Has(out, "Enter a number from 0 to 10.\n"));
	CHECK(Has(out, "Enter a whole number.\n"));
	CHECK(Has(out, "Enter a real date such as 2026-10-20.\n"));
	CHECK(Has(out, "Only manga have volumes.\n"));
	CHECK(catalist::test::ReadText("catalist.csv") == before);
}

TEST_CASE_FIXTURE(MenuFixture, "menu applies the status rules") {
	Seed();
	CHECK(Run("4\nodd taxi\n5\n3\n\n5\n2\n\n0\n0\n") == 0);
	CHECK(Has(out, "Error: Odd Taxi is completed; choose Rewatching to start it again\n"));
	CHECK(SavedTitle(MediaType::kAnime, "Odd Taxi").status == WatchStatus::kCompleted);
	CHECK(Run("4\nberserk\n5\n6\n\n0\n0\n") == 0);
	CHECK(Has(out, "Error: Berserk is not completed; choose Reading to carry on\n"));
	CHECK(Run("4\nodd taxi\n1\n\n0\n0\n") == 0);
	CHECK(Has(out, "Odd Taxi is at its total, 13 episodes. If there are more, use Set Total.\n"));
}

TEST_CASE_FIXTURE(MenuFixture, "menu finds a title from a typo or a list of matches") {
	Seed();
	CHECK(Run("4\ntaxo\ny\n4\n2\n\n0\n0\n") == 0);
	CHECK(SavedTitle(MediaType::kAnime, "Odd Taxi").score == 2);
	CHECK(Run("4\nnaruto\n\n0\n") == 0);
	CHECK(Has(out, "Nothing in your lists matches 'naruto'.\n"));
	CHECK(Run("4\nfrieren\n2\n4\n7\n\n0\n0\n") == 0);
	CHECK(Has(out, "Sousou no Frieren"));
	CHECK(Run("4\nfrieren\n0\n\n0\n") == 0);
}

TEST_CASE_FIXTURE(MenuFixture, "menu removes a title only when confirmed") {
	Seed();
	CHECK(Run("5\ntaxi\nn\n\n0\n") == 0);
	CHECK(Saved(MediaType::kAnime).size() == 2);
	CHECK(Run("5\ntaxi\ny\n\n0\n") == 0);
	CHECK(Has(out, "Odd Taxi (anime) removed.\n"));
	CHECK(Saved(MediaType::kAnime).size() == 1);
}

TEST_CASE_FIXTURE(MenuFixture, "menu shows the numbers of the lists") {
	Seed();
	CHECK(Run("6\n\n0\n") == 0);
	CHECK(Has(out, "Catalist / Stats"));
	CHECK(Has(out, "Titles         2\n"));
	CHECK(Has(out, "Days spent"));
}

TEST_CASE_FIXTURE(MenuFixture, "menu exports to a file and imports it") {
	Seed();
	CHECK(Run("7\n2\nanime.xml\n\n0\n0\n") == 0);
	CHECK(Has(out, "Saved anime.xml.\n"));
	CHECK(Has(catalist::test::ReadText("anime.xml"), "Odd Taxi"));

	Put("anime.xml", "keep me");
	CHECK(Run("7\n2\nanime.xml\nn\n\n0\n0\n") == 0);
	CHECK(catalist::test::ReadText("anime.xml") == "keep me");
	CHECK(Run("7\n2\nanime.xml\ny\n\n0\n0\n") == 0);
	CHECK(Has(catalist::test::ReadText("anime.xml"), "Odd Taxi"));

	std::filesystem::remove("catalist.csv");
	CHECK(Run("7\n1\nanime.xml\n\n0\n0\n") == 0);
	CHECK(Has(out, "2 titles, 2 added, 0 in your lists already.\n"));
	CHECK(Saved(MediaType::kAnime).size() == 2);
}

TEST_CASE_FIXTURE(MenuFixture, "menu import reports a file it cannot use") {
	Seed();
	std::string before = catalist::test::ReadText("catalist.csv");
	Put("bad.xml", "not a list");
	Put("packed.xml", "\x1f\x8b rest");
	CHECK(Run("7\n1\nmissing.xml\n\n1\nbad.xml\n\n1\npacked.xml\n\n0\n0\n") == 0);
	CHECK(Has(out, "Error: cannot read missing.xml\n"));
	CHECK(Has(out, "Error: no anime or manga found in bad.xml\n"));
	CHECK(Has(out, "is compressed"));
	CHECK(catalist::test::ReadText("catalist.csv") == before);
}

TEST_CASE_FIXTURE(MenuFixture, "menu import asks title by title where the file differs") {
	Put("anime.xml", catalist::test::kAnimeXml);
	REQUIRE(Run("7\n1\nanime.xml\n\n0\n0\n") == 0);
	REQUIRE(Run("4\ntaxi\n4\n3\n\n3\n9\n\n0\n4\nfrieren\n4\n4\n\n0\n0\n") == 0);

	CHECK(Run("7\n1\nanime.xml\nf\nq\n\n0\n0\n") == 0);
	CHECK(Has(out, "Cancelled. Nothing was imported.\n"));
	CHECK(SavedTitle(MediaType::kAnime, "Sousou no Frieren").score == 4);

	CHECK(Run("7\n1\nanime.xml\nx\nk\nf\n\n0\n0\n") == 0);
	CHECK(Has(out, "Sousou no Frieren (anime)\n  score     yours 4   file 10\n\n[k]eep"));
	CHECK(Has(out, "Odd Taxi (anime)\n  score     yours 3      file 8\n"
	               "  progress  yours 9/13   file 5/13\n"));
	CHECK(SavedTitle(MediaType::kAnime, "Sousou no Frieren").score == 4);
	CHECK(SavedTitle(MediaType::kAnime, "Odd Taxi").score == 8);

	CHECK(Run("7\n1\nanime.xml\nF\n\n0\n0\n") == 0);
	CHECK(SavedTitle(MediaType::kAnime, "Sousou no Frieren").score == 10);
}

TEST_CASE_FIXTURE(MenuFixture, "menu names statuses for the title's type") {
	Seed();
	CHECK(Run("4\ntaxi\n5\n0\n\n0\n0\n") == 0);
	CHECK(Has(out, "Catalist / Edit Title / Odd Taxi / Set Status"));
	CHECK(Has(out, "1. Plan to Watch\n2. Watching\n3. Completed\n4. On Hold\n5. Dropped\n"
	               "6. Rewatching\n0. Back\n"));
	CHECK(Run("4\nberserk\n5\n0\n\n0\n0\n") == 0);
	CHECK(Has(out, "1. Plan to Read\n2. Reading\n3. Completed\n4. On Hold\n5. Dropped\n"
	               "6. Rereading\n0. Back\n"));
	CHECK(Run("2\n5\n0\n0\n0\n") == 0);
	CHECK(Has(out, "1. Planned\n2. Watching/Reading\n3. Completed\n4. On Hold\n5. Dropped\n"
	               "6. Rewatching/Rereading\n0. Back\n"));
}

TEST_CASE_FIXTURE(MenuFixture, "menu puts the title in the edit breadcrumb and numbers ten options") {
	Seed();
	CHECK(Run("4\ntaxi\n0\n0\n") == 0);
	CHECK(Has(out, "Catalist / Edit Title / Odd Taxi\n"));
	CHECK(Has(out, " 1. +1 Progress\n 2. -1 Progress\n 3. Set Progress\n 4. Set Score\n"
	               " 5. Set Status\n 6. Set Total\n 7. Set Volumes\n 8. Set Start Date\n"
	               " 9. Set Finish Date\n10. Rename Title\n 0. Back\n"));
}

TEST_CASE_FIXTURE(MenuFixture, "menu log progress can reach every title by page") {
	{
		catalist::Catalogue db("catalist.csv");
		for (int i = 1; i <= 25; ++i)
			db.AddEntry(catalist::test::MakeEntry()
			                .MalId(i)
			                .Title(std::format("Show {:02}", i))
			                .Status(WatchStatus::kWatching));
		REQUIRE(db.Save("2026-11-24 10:00:00"));
	}
	CHECK(Run("3\n21\n5\n\n0\n") == 0);
	CHECK(Has(out, "Page 1 of 2\n21. Next Page\n22. Previous Page\n 0. Back\n"));
	CHECK(Has(out, "Page 2 of 2\n6. Next Page\n7. Previous Page\n0. Back\n"));
	CHECK(SavedTitle(MediaType::kAnime, "Show 25").progress == 1);
	CHECK(Run("3\n21\n7\n1\n\n0\n") == 0);
	CHECK(SavedTitle(MediaType::kAnime, "Show 25").progress == 1);
	CHECK_FALSE(Has(out, "type more of the name"));
}
