#include <doctest/doctest.h>

#include <algorithm>
#include <filesystem>
#include <format>

#include "support/menu_fixture.hpp"

using catalist::MediaType;
using catalist::WatchStatus;
using catalist::test::Has;
using catalist::test::MenuFixture;

namespace {

// What follows the last time `part` was drawn.
std::string After(const std::string& text, const std::string& part) {
	auto at = text.rfind(part);
	return at == std::string::npos ? "" : text.substr(at);
}

// The list as it was drawn last.
std::string LastList(const std::string& text) {
	auto anime = text.rfind("Catalist / Anime\n");
	auto manga = text.rfind("Catalist / Manga\n");
	if (anime == std::string::npos)
		return manga == std::string::npos ? "" : text.substr(manga);
	return text.substr(manga == std::string::npos ? anime : std::max(anime, manga));
}

void AddShows(int count, WatchStatus status) {
	catalist::Catalogue db("catalist.csv");
	for (int i = 1; i <= count; ++i)
		db.AddEntry(catalist::test::MakeEntry()
		                .MalId(i)
		                .Title(std::format("Show {:02}", i))
		                .Status(status));
	REQUIRE(db.Save("2026-11-24 10:00:00"));
}

}  // namespace

TEST_CASE_FIXTURE(MenuFixture, "menu shows the main menu and exits with 0") {
	Seed();
	CHECK(Run("0\n") == 0);
	CHECK(out.starts_with("    ___       ___"));
	CHECK(Has(out, "  1  Anime\n  2  Manga\n  3  Import & Export\n  0  Exit\n"));
	CHECK(Has(out, "> Enter choice [0-3]: "));
	CHECK(Has(out, "  2 anime    2 manga    2 in progress\n"));
	CHECK(Has(out, "Goodbye!\n"));
}

TEST_CASE_FIXTURE(MenuFixture, "menu draws the big logo only when it fits") {
	CHECK(Run("0\n") == 0);
	CHECK_FALSE(Has(out, R"(/::\~\:\)"));
	columns = 120;
	CHECK(Run("0\n") == 0);
	CHECK(Has(out, R"(/::\~\:\)"));
	columns = 60;
	CHECK(Run("0\n") == 0);
	CHECK(out.starts_with("  0 anime"));
}

TEST_CASE_FIXTURE(MenuFixture, "menu says what to do when the lists are empty") {
	CHECK(Run("0\n") == 0);
	CHECK(Has(out, "Your lists are empty."));
	CHECK(Run("1\n0\n0\n") == 0);
	CHECK(Has(out, "Your anime list is empty. Choose 6 to add a title."));
	CHECK_FALSE(std::filesystem::exists("catalist.csv"));
}

TEST_CASE_FIXTURE(MenuFixture, "menu asks again after a choice that is not on it") {
	Seed();
	CHECK(Run("9\nabc\n-1\n\n0\n") == 0);
	CHECK(Has(out, "! Enter a number from 0 to 3.\n"));
	CHECK(Has(out, "Goodbye!\n"));
}

TEST_CASE_FIXTURE(MenuFixture, "menu ends when the input ends, wherever it is") {
	Seed();
	for (const char* typed :
	     {"", "1\n", "1\n1\n", "1\n2\n", "1\n1\n1\n", "1\n1\n1\n2\n", "1\n1\n1\n3\n", "1\n1\n1\n5\n",
	      "1\n1\n1\n5\n1\n", "1\n1\n1\n6\n", "1\n3\n", "1\n3\n2\n", "1\n4\n", "1\n6\n", "1\n7\n",
	      "2\n", "3\n", "3\n1\n"}) {
		CAPTURE(typed);
		CHECK(Run(typed) == 0);
		CHECK(Has(out, "Goodbye!\n"));
	}
	CHECK(SavedTitle(MediaType::kAnime, "Odd Taxi").progress == 5);
	CHECK(Saved(MediaType::kAnime).size() == 2);
}

TEST_CASE_FIXTURE(MenuFixture, "menu lists one kind, numbered, with its menu under it") {
	Seed();
	CHECK(Run("1\n0\n0\n") == 0);
	CHECK(Has(out, "Catalist / Anime\n"));
	CHECK(Has(out, "  2 of 2 titles   Sorted by Last Updated\n"));
	CHECK(Has(out,
	          "  1  [W]  Odd Taxi           TV  ****-   5/13\n"
	          "  2  [C]  Sousou no Frieren  -   *****  28/28\n"));
	CHECK(Has(out,
	          "  1  Open a Title         6  Add from AniList\n"
	          "  2  Search               7  Stats\n"
	          "  3  Filter               8  Next Page\n"
	          "  4  Sort                 9  Previous Page\n"
	          "  5  Clear Search/Filter  0  Back\n"));
	CHECK(Has(out, "> Enter choice [0-9]: "));
	CHECK(Run("2\n0\n0\n") == 0);
	CHECK(Has(out, "Catalist / Manga\n"));
	CHECK(Has(out, "  1  [W]  Berserk            -  364/?\n"));
	CHECK_FALSE(Has(out, "Odd Taxi"));
}

TEST_CASE_FIXTURE(MenuFixture, "menu searches the list, and offers the closest title") {
	Seed();
	CHECK(Run("1\n2\ntaxi odd\n0\n0\n") == 0);
	CHECK(Has(LastList(out),
	          "titles: 'taxi odd'   Sorted by Last Updated\n\n"
	          "  1  [W]  Odd Taxi  TV  ****-  5/13\n"));
	CHECK(Run("1\n2\ntaxo\n0\n0\n") == 0);
	CHECK(Has(out, "! Nothing matches 'taxo'. Did you mean Odd Taxi?\n"));
	CHECK(Run("1\n2\nnaruto\n0\n0\n") == 0);
	CHECK(Has(out, "! Nothing matches 'naruto'.\n"));
}

TEST_CASE_FIXTURE(MenuFixture, "menu filters by status, score and format, and clears them") {
	Seed();
	CHECK(Run("1\n3\n1\n3\n0\n0\n") == 0);
	CHECK(Has(out, "Catalist / Anime / Filter / By Status\n"));
	CHECK(Has(LastList(out), "1 of 2 titles: Completed"));
	CHECK(Has(LastList(out), "Sousou no Frieren"));
	CHECK(Run("1\n3\n2\n8\n0\n0\n") == 0);
	CHECK(Has(out, "1 of 2 titles: score 8"));
	CHECK(Run("1\n3\n2\n0\n0\n0\n") == 0);
	CHECK(Has(out, "0 of 2 titles: no score"));
	CHECK(Has(out, "No titles match. Choose 5 to clear the search and filters.\n"));
	CHECK(Run("1\n3\n2\n11\n\n0\n0\n") == 0);
	CHECK(Has(out, "! Enter a number from 0 to 10.\n"));
	CHECK(Run("1\n3\n3\n1\n3\n4\n0\n0\n") == 0);
	CHECK(Has(out, "1 of 2 titles: TV"));
	CHECK(Has(LastList(out), "2 of 2 titles   Sorted"));
	CHECK(Run("1\n2\ntaxi\n3\n1\n2\n5\n0\n0\n") == 0);
	CHECK(Has(out, "1 of 2 titles: 'taxi', Watching"));
	CHECK(Has(LastList(out), "2 of 2 titles   Sorted"));
}

TEST_CASE_FIXTURE(MenuFixture, "menu gives manga no format filter") {
	Seed();
	CHECK(Run("2\n3\n0\n0\n0\n") == 0);
	CHECK(Has(out, "  1  By Status\n  2  By Score\n  3  Clear Filters\n  0  Back\n"));
	CHECK(Run("2\n3\n1\n1\n3\n3\n0\n0\n") == 0);
	CHECK(Has(out, "1 of 2 titles: Plan to Read"));
	CHECK(Has(LastList(out), "2 of 2 titles   Sorted"));
}

TEST_CASE_FIXTURE(MenuFixture, "menu sorts six ways and reverses") {
	Seed();
	CHECK(Run("1\n4\n0\n0\n0\n") == 0);
	CHECK(Has(out,
	          "  1  Last Updated\n  2  Title\n  3  Status\n  4  Score\n  5  Progress\n"
	          "  6  Start Date\n  7  Reverse Order\n  0  Back\n"));
	CHECK(Run("1\n4\n5\n0\n0\n") == 0);
	auto list = After(out, "Sorted by");
	CHECK(list.starts_with("Sorted by Progress\n"));
	CHECK(list.find("Sousou") < list.find("Odd Taxi"));
	CHECK(Run("1\n4\n5\n4\n7\n0\n0\n") == 0);
	list = After(out, "Sorted by");
	CHECK(list.starts_with("Sorted by Progress, reversed\n"));
	CHECK(list.find("Odd Taxi") < list.find("Sousou"));
}

TEST_CASE_FIXTURE(MenuFixture, "menu pages a long list") {
	AddShows(45, WatchStatus::kPlanToWatch);
	CHECK(Run("1\n9\n8\n8\n8\n0\n0\n") == 0);
	CHECK(Has(out, "! This is the first page.\n"));
	CHECK(Has(out, "Page 1 of 3"));
	CHECK(Has(After(out, "Sorted by"), "  5  [P]  Show 45"));
	CHECK(Has(out, "Page 3 of 3"));
	CHECK(Has(out, "! This is the last page.\n"));
	CHECK(Run("1\n8\n1\n21\n9\n1\n20\n0\n0\n0\n") == 0);
	CHECK(Has(out, "! There is no title 21 on this page.\n"));
	CHECK(Has(out, "Catalist / Anime / Show 20\n"));
}

TEST_CASE_FIXTURE(MenuFixture, "menu opens a title with its card and actions") {
	Seed();
	CHECK(Run("1\n1\n1\n0\n0\n0\n") == 0);
	CHECK(Has(out, "Catalist / Anime / Odd Taxi\n"));
	CHECK(Has(out,
	          "  Format        TV\n  Status        [W] Watching\n"
	          "  Score         ****-  8/10\n"
	          "  Progress      [########------------]  5/13 episodes\n"));
	CHECK(Has(out, "  MyAnimeList   44265\n"));
	CHECK(Has(out,
	          "  1  +1 Episode\n  2  Set Progress\n  3  Set Status\n  4  Set Score\n"
	          "  5  Edit Details\n  6  Remove\n  0  Back\n"));
	CHECK(Run("2\n1\n1\n0\n0\n0\n") == 0);
	CHECK(Has(out, "  1  +1 Chapter\n"));
	CHECK_FALSE(Has(out, "Format"));
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
	CHECK(Run("1\n0\n0\n") == 0);
	CHECK(Has(out, "Frieren: Beyond Journey's End"));
	CHECK_FALSE(Has(out, "Sousou"));
	CHECK(Run("1\n2\nbeyond\n1\n1\n0\n0\n0\n") == 0);
	CHECK(Has(out, "  Romaji        Sousou no Frieren\n"));
}

TEST_CASE_FIXTURE(MenuFixture, "menu counts the next episode and stays on the title") {
	Seed();
	CHECK(Run("1\n1\n1\n1\n\n1\n\n0\n0\n0\n") == 0);
	CHECK(Has(out, "OK Odd Taxi  progress 5 -> 6/13\n"));
	CHECK(Has(out, "OK Odd Taxi  progress 6 -> 7/13\n"));
	CHECK(SavedTitle(MediaType::kAnime, "Odd Taxi").progress == 7);
}

TEST_CASE_FIXTURE(MenuFixture, "menu edits a title from its page") {
	Seed();
	CHECK(Run("1\n1\n1\n2\n10\n\n4\n9\n\n5\n1\n20\n\n2\n2026-01-02\n\n4\nOdd Cab\n\n0\n0\n0\n0\n") ==
	      0);
	CHECK(Has(out,
	          "  1  Set Total\n  2  Set Start Date\n  3  Set Finish Date\n  4  Rename\n"
	          "  0  Back\n"));
	auto taxi = SavedTitle(MediaType::kAnime, "Odd Cab");
	CHECK(taxi.progress == 10);
	CHECK(taxi.score == 9);
	CHECK(taxi.total_units == 20);
	CHECK(catalist::FormatDate(taxi.start_date) == "2026-01-02");
	CHECK(taxi.title == "Odd Taxi");
	CHECK(Run("2\n1\n1\n5\n2\n4\n\n0\n0\n0\n0\n") == 0);
	CHECK(Has(out, "  1  Set Total\n  2  Set Volumes Read\n"));
	CHECK(SavedTitle(MediaType::kManga, "Berserk").volumes == 4);
}

TEST_CASE_FIXTURE(MenuFixture, "menu refuses bad values and keeps going") {
	Seed();
	std::string before = catalist::test::ReadText("catalist.csv");
	CHECK(Run("1\n1\n1\n4\n11\n\n2\nlots\n\n5\n2\n2026-13-40\n\n0\n0\n0\n0\n") == 0);
	CHECK(Has(out, "! Enter a number from 0 to 10.\n"));
	CHECK(Has(out, "! Enter a whole number.\n"));
	CHECK(Has(out, "! Enter a real date such as 2026-10-20.\n"));
	CHECK(catalist::test::ReadText("catalist.csv") == before);
}

TEST_CASE_FIXTURE(MenuFixture, "menu applies the status rules") {
	Seed();
	CHECK(Run("1\n1\n1\n3\n3\n\n3\n2\n\n1\n\n0\n0\n0\n") == 0);
	CHECK(Has(out, "Error: Odd Taxi is completed; choose Rewatching to start it again\n"));
	CHECK(Has(out,
	          "- Odd Taxi is at its total, 13 episodes. If there are more, set the total in "
	          "Edit Details.\n"));
	auto taxi = SavedTitle(MediaType::kAnime, "Odd Taxi");
	CHECK(taxi.status == WatchStatus::kCompleted);
	CHECK(taxi.progress == 13);
	CHECK(Run("2\n1\n1\n3\n6\n\n0\n0\n0\n") == 0);
	CHECK(Has(out, "Error: Berserk is not completed; choose Reading to carry on\n"));
}

TEST_CASE_FIXTURE(MenuFixture, "menu names statuses for the title's type") {
	Seed();
	CHECK(Run("1\n1\n1\n3\n0\n\n0\n0\n0\n") == 0);
	CHECK(Has(out, "Catalist / Anime / Odd Taxi / Set Status\n"));
	CHECK(Has(out,
	          "  1  [P] Plan to Watch\n  2  [W] Watching\n  3  [C] Completed\n"
	          "  4  [H] On Hold\n  5  [D] Dropped\n  6  [R] Rewatching\n  0  Back\n"));
	CHECK(Run("2\n1\n1\n3\n0\n\n0\n0\n0\n") == 0);
	CHECK(Has(out,
	          "  1  [P] Plan to Read\n  2  [W] Reading\n  3  [C] Completed\n"
	          "  4  [H] On Hold\n  5  [D] Dropped\n  6  [R] Rereading\n  0  Back\n"));
}

TEST_CASE_FIXTURE(MenuFixture, "menu removes a title only when confirmed") {
	Seed();
	CHECK(Run("1\n1\n1\n6\nn\n\n0\n0\n0\n") == 0);
	CHECK(Has(out, "Remove Odd Taxi from your anime list? (y/n): "));
	CHECK(Has(out, "- Nothing was removed.\n"));
	CHECK(Saved(MediaType::kAnime).size() == 2);
	CHECK(Run("1\n1\n1\n6\ny\n\n0\n0\n") == 0);
	CHECK(Has(out, "OK Odd Taxi removed.\n"));
	CHECK(Has(LastList(out), "1 of 1 titles"));
	CHECK(Saved(MediaType::kAnime).size() == 1);
}

TEST_CASE_FIXTURE(MenuFixture, "menu shows the numbers of one kind") {
	Seed();
	CHECK(Run("1\n7\n\n0\n0\n") == 0);
	CHECK(Has(out, "Catalist / Anime / Stats\n"));
	CHECK(Has(out, "  Titles                2\n"));
	CHECK(Has(out, "  [W] Watching          1      [##########----------]\n"));
	CHECK(Has(out, "  Mean score            9.00  ****+\n"));
	CHECK(Has(out, "Days spent"));
	CHECK(Run("2\n7\n\n0\n0\n") == 0);
	CHECK(Has(out, "  Chapters              364\n"));
}

TEST_CASE_FIXTURE(MenuFixture, "menu adds a title with the status chosen") {
	KnowTitle("Dandadan", 57334);
	CHECK(Run("1\n6\nDandadan\n2\n\n0\n0\n") == 0);
	CHECK(Has(out, "Catalist / Anime / Add / Dandadan\n"));
	CHECK(Has(out, "OK Dandadan added as Watching.\n"));
	auto added = SavedTitle(MediaType::kAnime, "Dandadan");
	CHECK(added.status == WatchStatus::kWatching);
	CHECK(added.mal_id == 57334);
	CHECK(Has(LastList(out), "1 of 1 titles"));

	CHECK(Run("1\n6\nDandadan\n\n0\n0\n") == 0);
	CHECK(Has(out, "- Dandadan is in your anime list already (Watching).\n"));
	CHECK(Saved(MediaType::kAnime).size() == 1);
}

TEST_CASE_FIXTURE(MenuFixture, "menu add numbers the matches to choose from") {
	web.Respond(R"("search":"bers")", R"({"data":{
	  "manga":{"media":[{"id":30002,"idMal":2,"title":{"romaji":"Berserk"},
	    "startDate":{"year":1989}},{"id":30003,"idMal":3,"title":{"romaji":"Berserk Prototype"},
	    "chapters":1,"startDate":{"year":1988}}]}}})");
	CHECK(Run("2\n6\nbers\n2\n1\n\n0\n0\n") == 0);
	CHECK(Has(out,
	          "  1  1989  Berserk            ? chapters  -\n"
	          "  2  1988  Berserk Prototype   1 chapter  -\n"));
	CHECK(SavedTitle(MediaType::kManga, "Berserk Prototype").mal_id == 3);
	CHECK(Run("2\n6\nbers\n0\n0\n0\n") == 0);
	CHECK(Saved(MediaType::kManga).size() == 1);
}

TEST_CASE_FIXTURE(MenuFixture, "menu add takes the one exact name without asking") {
	web.Respond(R"("search":"frieren beyond journeys end")", R"({"data":{"anime":{"media":[
	  {"id":154587,"idMal":52991,"title":{"romaji":"Sousou no Frieren",
	   "english":"Frieren: Beyond Journey's End"},"episodes":28},
	  {"id":182255,"idMal":59978,"title":{"romaji":"Sousou no Frieren 2nd Season"}}]}}})");
	CHECK(Run("1\n6\nfrieren beyond journeys end\n1\n\n0\n0\n") == 0);
	CHECK(SavedTitle(MediaType::kAnime, "Frieren: Beyond Journey's End").mal_id == 52991);
}

TEST_CASE_FIXTURE(MenuFixture, "menu add says when AniList cannot help") {
	CHECK(Run("1\n6\ndandadan\n\n0\n0\n") == 0);
	CHECK(Has(out, "Error: could not reach AniList"));
	web.Respond(R"("search":"zzz")", R"({"data":{"anime":{"media":[]}}})");
	CHECK(Run("1\n6\nzzz\n\n0\n0\n") == 0);
	CHECK(Has(out, "- AniList has no anime matching 'zzz'.\n"));
	CHECK_FALSE(std::filesystem::exists("catalist.csv"));
}

TEST_CASE_FIXTURE(MenuFixture, "menu exports to a file and imports it") {
	Seed();
	CHECK(Run("3\n2\nanime.xml\n\n0\n0\n") == 0);
	CHECK(Has(out, "  1  Import XML\n  2  Export Anime\n  3  Export Manga\n  0  Back\n"));
	CHECK(Has(out, "OK Saved anime.xml.\n"));
	CHECK(Has(catalist::test::ReadText("anime.xml"), "Odd Taxi"));

	Put("anime.xml", "keep me");
	CHECK(Run("3\n2\nanime.xml\nn\n\n0\n0\n") == 0);
	CHECK(catalist::test::ReadText("anime.xml") == "keep me");
	CHECK(Run("3\n2\nanime.xml\ny\n\n0\n0\n") == 0);
	CHECK(Has(catalist::test::ReadText("anime.xml"), "Odd Taxi"));

	std::filesystem::remove("catalist.csv");
	CHECK(Run("3\n1\nanime.xml\n\n0\n0\n") == 0);
	CHECK(Has(out, "2 titles, 2 added, 0 in your lists already.\n"));
	CHECK(Saved(MediaType::kAnime).size() == 2);
}

TEST_CASE_FIXTURE(MenuFixture, "menu import reports a file it cannot use") {
	Seed();
	std::string before = catalist::test::ReadText("catalist.csv");
	Put("bad.xml", "not a list");
	Put("packed.xml", "\x1f\x8b rest");
	CHECK(Run("3\n1\nmissing.xml\n\n1\nbad.xml\n\n1\npacked.xml\n\n0\n0\n") == 0);
	CHECK(Has(out, "Error: cannot read missing.xml\n"));
	CHECK(Has(out, "Error: no anime or manga found in bad.xml\n"));
	CHECK(Has(out, "is compressed"));
	CHECK(catalist::test::ReadText("catalist.csv") == before);
}

TEST_CASE_FIXTURE(MenuFixture, "menu import asks title by title where the file differs") {
	Put("anime.xml", catalist::test::kAnimeXml);
	REQUIRE(Run("3\n1\nanime.xml\n\n0\n0\n") == 0);
	REQUIRE(Run("1\n1\n1\n4\n3\n\n2\n9\n\n0\n1\n2\n4\n4\n\n0\n0\n0\n") == 0);
	REQUIRE(SavedTitle(MediaType::kAnime, "Odd Taxi").score == 3);
	REQUIRE(SavedTitle(MediaType::kAnime, "Sousou no Frieren").score == 4);

	CHECK(Run("3\n1\nanime.xml\nf\nq\n\n0\n0\n") == 0);
	CHECK(Has(out, "Cancelled. Nothing was imported.\n"));
	CHECK(SavedTitle(MediaType::kAnime, "Sousou no Frieren").score == 4);

	CHECK(Run("3\n1\nanime.xml\nx\nk\nf\n\n0\n0\n") == 0);
	CHECK(Has(out,
	          "Sousou no Frieren (anime)\n  score     yours 4   file 10\n\n"
	          "k Keep  f File  r Keep Rest  t File Rest  q Quit: "));
	CHECK(Has(out,
	          "Odd Taxi (anime)\n  score     yours 3      file 8\n"
	          "  progress  yours 9/13   file 5/13\n"));
	CHECK(SavedTitle(MediaType::kAnime, "Sousou no Frieren").score == 4);
	CHECK(SavedTitle(MediaType::kAnime, "Odd Taxi").score == 8);

	CHECK(Run("3\n1\nanime.xml\nF\n\n0\n0\n") == 0);
	CHECK(SavedTitle(MediaType::kAnime, "Sousou no Frieren").score == 10);
}
