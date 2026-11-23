#include <doctest/doctest.h>

#include <filesystem>
#include <string>

#include "storage/mal_xml.hpp"
#include "support/cli_fixture.hpp"

using catalist::MediaType;
using catalist::WatchStatus;
using catalist::test::CliFixture;
using catalist::test::Has;
using catalist::test::kAnimeXml;
using catalist::test::kMangaXml;
using catalist::test::MakeEntry;

TEST_CASE_FIXTURE(CliFixture, "cli import adds the titles and saves them") {
	Put("lists/anime.xml", kAnimeXml);
	Put("lists/manga.xml", kMangaXml);

	CHECK(Run({"import", "lists/anime.xml", "lists/manga.xml"}) == 0);
	CHECK(out ==
	      "a:1\tSousou no Frieren\tadded\t-\tcompleted\n"
	      "a:2\tOdd Taxi\tadded\t-\twatching\n"
	      "m:1\tBerserk\tadded\t-\twatching\n");
	CHECK(err ==
	      "lists/anime.xml: 2 titles, 2 added, 0 in your lists already\n"
	      "lists/manga.xml: 1 title, 1 added, 0 in your lists already\n");

	auto anime = Saved(MediaType::kAnime);
	REQUIRE(anime.size() == 2);
	CHECK(anime[0].title == "Sousou no Frieren");
	CHECK(anime[0].mal_id == 52991);
	CHECK(anime[0].score == 10);
	CHECK(anime[0].progress == 28);
	CHECK(anime[0].status == catalist::WatchStatus::kCompleted);
	CHECK(anime[1].title == "Odd Taxi");
	auto manga = Saved(MediaType::kManga);
	REQUIRE(manga.size() == 1);
	CHECK(manga[0].title == "Berserk");
	CHECK(manga[0].progress == 364);

	// All the files of one command are one step for undo.
	REQUIRE(Steps().size() == 1);
	CHECK(Steps()[0].label == "import lists/anime.xml lists/manga.xml");
	CHECK(Steps()[0].changes == 3);
	CHECK(Run({"undo", "-q"}) == 0);
	CHECK(Saved(MediaType::kAnime).empty());
	CHECK(Saved(MediaType::kManga).empty());
}

TEST_CASE_FIXTURE(CliFixture, "cli import reads standard input for a FILE of -") {
	CHECK(Run({"import", "-"}, kAnimeXml) == 0);
	CHECK(Has(out, "\tOdd Taxi\tadded\t-\twatching\n"));
	CHECK(err == "standard input: 2 titles, 2 added, 0 in your lists already\n");
	CHECK(Saved(MediaType::kAnime).size() == 2);
	// -q prints nothing, not even that.
	CHECK(Run({"import", "-q", "-"}, kAnimeXml) == 0);
	CHECK(out.empty());
	CHECK(err.empty());
	CHECK(Run({"import", "-"}, "not an export") == 1);
	CHECK(err ==
	      "error: no anime or manga found in standard input (is it a MyAnimeList XML export?)\n");
	CHECK(Run({"import", "-"}, std::string("\x1f\x8b\x08\x00", 4) + "gz") == 1);
	CHECK(err == "error: standard input is compressed; unpack it first (gunzip)\n");
}

TEST_CASE_FIXTURE(CliFixture, "cli import adds only the titles not tracked yet") {
	Seed();
	Put("lists/anime.xml", kAnimeXml);
	// Both titles are there already.
	CHECK(Run({"import", "lists/anime.xml"}) == 0);
	CHECK(out.empty());
	CHECK(Has(err, "2 titles, 0 added, 2 in your lists already\n"));
	Put("lists/manga.xml",
	    "<myanimelist><manga><manga_mangadb_id>656</manga_mangadb_id>"
	    "<manga_title><![CDATA[Vagabond]]></manga_title>"
	    "<my_status>Plan to Read</my_status></manga></myanimelist>");
	CHECK(Run({"import", "lists/manga.xml"}) == 0);
	CHECK(out == "m:3\tVagabond\tadded\t-\tplanned\n");
}

TEST_CASE_FIXTURE(CliFixture, "cli importing again keeps what the user changed") {
	Put("lists/anime.xml", kAnimeXml);
	REQUIRE(Run({"import", "lists/anime.xml"}) == 0);

	{
		catalist::Catalogue db("data");
		REQUIRE(db.LoadAll().has_value());
		auto taxi = db.GetEntries(MediaType::kAnime)[1];
		REQUIRE(taxi.title == "Odd Taxi");
		taxi.score = 9;
		taxi.progress = 13;
		taxi.start_date = catalist::test::RequireDate("2026-01-02");
		REQUIRE(db.UpdateEntry(taxi));
		REQUIRE(db.SaveAll());
	}

	CHECK(Run({"import", "lists/anime.xml"}) == 0);
	CHECK(out.empty());
	CHECK(err ==
	      "lists/anime.xml: 2 titles, 0 added, 2 in your lists already\n"
	      "Nothing changed.\n");
	auto anime = Saved(MediaType::kAnime);
	REQUIRE(anime.size() == 2);
	CHECK(anime[1].score == 9);
	CHECK(anime[1].progress == 13);
	CHECK(catalist::FormatDate(anime[1].start_date) == "2026-01-02");
	CHECK(Steps().size() == 1);
}

TEST_CASE_FIXTURE(CliFixture, "cli importing twice adds a title without a site id once") {
	Put("lists/noid.xml",
	    "<myanimelist><anime><series_title><![CDATA[Home Video]]></series_title>"
	    "<my_watched_episodes>3</my_watched_episodes><my_status>Watching</my_status></anime>"
	    "</myanimelist>");
	CHECK(Run({"import", "lists/noid.xml"}) == 0);
	CHECK(Run({"import", "lists/noid.xml"}) == 0);
	CHECK(err ==
	      "lists/noid.xml: 1 title, 0 added, 1 in your lists already\n"
	      "Nothing changed.\n");
	REQUIRE(Saved(MediaType::kAnime).size() == 1);
	CHECK(Saved(MediaType::kAnime)[0].progress == 3);

	// A stored title with a site id is another title, whatever its name.
	Put("lists/taxi.xml", kAnimeXml);
	CHECK(Run({"import", "lists/taxi.xml"}) == 0);
	Put("lists/same_name.xml",
	    "<myanimelist><anime><series_title><![CDATA[odd taxi]]></series_title>"
	    "<my_status>Watching</my_status></anime></myanimelist>");
	CHECK(Run({"import", "lists/same_name.xml"}) == 0);
	CHECK(Has(err, "1 title, 1 added"));
	CHECK(Saved(MediaType::kAnime).size() == 4);
}

TEST_CASE_FIXTURE(CliFixture, "cli import changes nothing when any file is bad") {
	Put("lists/anime.xml", kAnimeXml);
	Put("lists/empty.xml", "<myanimelist></myanimelist>");

	CHECK(Run({"import", "lists/anime.xml", "lists/missing.xml"}) == 1);
	CHECK(err == "error: cannot read lists/missing.xml\n");
	CHECK(out.empty());
	CHECK(Saved(MediaType::kAnime).empty());

	Put("lists/anime.xml.gz", std::string("\x1f\x8b\x08\x00", 4) + "gz");
	CHECK(Run({"import", "lists/anime.xml.gz"}) == 1);
	CHECK(err ==
	      "error: lists/anime.xml.gz is compressed; unpack it first "
	      "(gunzip lists/anime.xml.gz)\n");

	CHECK(Run({"import", "lists/anime.xml", "lists/empty.xml"}) == 1);
	CHECK(err ==
	      "error: no anime or manga found in lists/empty.xml (is it a "
	      "MyAnimeList XML export?)\n");
	CHECK(Saved(MediaType::kAnime).empty());
}

TEST_CASE_FIXTURE(CliFixture, "cli import exits 1 when the catalogue cannot be saved") {
	Put("lists/anime.xml", kAnimeXml);
	catalist::test::MakeUnwritable("data/catalist.db");
	CHECK(Run({"import", "lists/anime.xml"}) == 1);
	CHECK(err.find("error: cannot open data/catalist.db: ") == 0);
	CHECK(Run({"add", "--no-lookup", "Home Video"}) == 1);
	CHECK(err.find("error: cannot open data/catalist.db: ") == 0);
	CHECK(out.empty());
}

TEST_CASE_FIXTURE(CliFixture, "cli export prints what import read") {
	Put("lists/anime.xml", kAnimeXml);
	Put("lists/manga.xml", kMangaXml);
	REQUIRE(Run({"import", "lists/anime.xml", "lists/manga.xml"}) == 0);

	CHECK(Run({"export", "anime"}) == 0);
	CHECK(err.empty());
	auto anime = catalist::mal_xml::Parse(out);
	REQUIRE(anime.size() == 2);
	CHECK(anime[0].title == "Sousou no Frieren");
	CHECK(anime[0].mal_id == 52991);
	CHECK(anime[0].progress == 28);
	CHECK(anime[1].title == "Odd Taxi");
	CHECK(anime[1].score == 8);

	CHECK(Run({"export", "manga"}) == 0);
	auto manga = catalist::mal_xml::Parse(out);
	REQUIRE(manga.size() == 1);
	CHECK(manga[0].type == MediaType::kManga);
	CHECK(manga[0].title == "Berserk");
}

TEST_CASE_FIXTURE(CliFixture, "cli export wants a list") {
	Seed();
	CHECK(Run({"export"}) == 2);
	CHECK(err.find("error: say which list: anime or manga\n"
	               "Usage: catalist export ") == 0);
	CHECK(Run({"export", "books"}) == 2);
	CHECK(Run({"export", "anime", "manga"}) == 2);
	CHECK(Run({"export", "--format=csv", "anime"}) == 2);
	CHECK(out.empty());
}

TEST_CASE_FIXTURE(CliFixture, "cli export notes the titles it had to leave out") {
	{
		catalist::Catalogue db("data");
		db.AddEntry(catalist::test::MakeEntry().Title("Home video"));
		db.AddEntry(catalist::test::MakeEntry().Title("Another one"));
		REQUIRE(db.SaveAll());
	}
	CHECK(Run({"export", "anime"}) == 0);
	CHECK(err == "note: 2 titles without a MyAnimeList id were left out.\n");
	CHECK(catalist::mal_xml::Parse(out).empty());
	CHECK(out.find("<myanimelist>") != std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture, "cli export of an empty catalogue is an empty list") {
	CHECK(Run({"export", "manga"}) == 0);
	CHECK(err.empty());
	CHECK(out.find("<user_export_type>2</user_export_type>") != std::string::npos);
	CHECK(catalist::mal_xml::Parse(out).empty());
	CHECK_FALSE(std::filesystem::exists("data"));
}
