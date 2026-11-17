#include <doctest/doctest.h>

#include <algorithm>
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

TEST_CASE_FIXTURE(CliFixture, "cli import reads the CSV files of Catalist 0.3.0") {
	// As 0.3.0 wrote them: the AniList id third, notes, and its sync columns.
	Put("old/anime.csv",
	    "#id,mal_id,anilist_id,type,title,status,score,progress,total_units,volumes,"
	    "total_volumes,rewatched,start_date,finish_date,notes,updated_at,dirty,external_ids,"
	    "queue_pos,synced\r\n"
	    "1,52991,154587,anime,Sousou no Frieren,Completed,10,28,28,0,0,0,2023-09-29,"
	    "2024-03-22,\"Best, \"\"ever\"\"\",2026-10-17,0,,0,\r\n");
	Put("old/manga.csv",
	    "#id,mal_id,type,title,status,score,progress,total_units,volumes,total_volumes,"
	    "rewatched,start_date,finish_date,updated_at,released,checked_at,anilist_id\n"
	    "2,2,manga,Berserk,Reading,0,364,0,42,0,1,2019-05-01,,2026-10-17,0,,30002\n");
	CHECK(Run({"import", "old/anime.csv", "old/manga.csv"}) == 0);
	CHECK(err ==
	      "old/anime.csv: 1 title, 1 added, 0 in your lists already\n"
	      "old/manga.csv: 1 title, 1 added, 0 in your lists already\n");
	auto frieren = SavedTitle(MediaType::kAnime, "Sousou no Frieren");
	CHECK(frieren.anilist_id == 154587);
	CHECK(frieren.score == 10);
	CHECK(catalist::FormatDate(frieren.finish_date) == "2024-03-22");
	auto berserk = SavedTitle(MediaType::kManga, "Berserk");
	CHECK(berserk.volumes == 42);
	CHECK(berserk.anilist_id == 30002);

	Put("old/bad.csv", "#id,mal_id\n52991,52991,Sousou no Frieren,Completed\n");
	CHECK(Run({"import", "old/bad.csv"}) == 1);
	CHECK(err == "error: old/bad.csv: row 1 has a column layout this version cannot read\n");
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
	// xml is the format when none is named.
	std::string xml = out;
	CHECK(Run({"export", "--format=xml", "anime"}) == 0);
	CHECK(out == xml);

	CHECK(Run({"export", "manga"}) == 0);
	auto manga = catalist::mal_xml::Parse(out);
	REQUIRE(manga.size() == 1);
	CHECK(manga[0].type == MediaType::kManga);
	CHECK(manga[0].title == "Berserk");
}

TEST_CASE_FIXTURE(CliFixture, "cli export --format=csv prints the columns of show, quoted as CSV") {
	Seed();
	REQUIRE(Run({"add", "--no-lookup", "Say \"hi\", twice"}) == 0);
	CHECK(Run({"export", "--format=csv", "anime"}) == 0);
	CHECK(err.empty());
	CHECK(out ==
	      "ref,type,title,status,score,progress,total,volumes,rewatched,started,finished,"
	      "updated,new,total_volumes,released,anilist_id,mal_id,url,kitsu_id\n"
	      "a:1,anime,Sousou no Frieren,completed,10,28,28,0,0,,,,0,,,154587,52991,"
	      "https://anilist.co/anime/154587,\n"
	      "a:2,anime,Odd Taxi,watching,8,5,13,0,0,2026-10-01,,,8,,13,128547,44265,"
	      "https://anilist.co/anime/128547,\n"
	      "a:3,anime,\"Say \"\"hi\"\", twice\",planned,0,0,,0,0,,,2026-10-20,0,,,,,,\n");
	// The same at a terminal: a file is what it is for.
	std::string csv = out;
	CHECK(Run({"export", "--format", "csv", "anime"}, "", 100) == 0);
	CHECK(out == csv);
	CHECK(Run({"export", "--format=csv", "manga"}) == 0);
	CHECK(std::count(out.begin(), out.end(), '\n') == 3);
}

TEST_CASE_FIXTURE(CliFixture, "cli export --format=json prints one object a title") {
	Seed();
	CHECK(Run({"export", "--format=json", "anime"}) == 0);
	CHECK(std::count(out.begin(), out.end(), '\n') == 2);
	CHECK(out.find("{\"ref\":\"a:1\",\"type\":\"anime\",\"title\":\"Sousou no Frieren\",") == 0);
	CHECK(
	    Has(out,
	        "\n{\"ref\":\"a:2\",\"type\":\"anime\",\"title\":\"Odd Taxi\",\"status\":\"watching\","
	        "\"score\":8,\"progress\":5,\"total\":13,"));
	// The lines of `show --json`, ids and all.
	std::string json = out;
	CHECK(Run({"show", "--json", "a:1", "a:2"}) == 0);
	CHECK(out == json);
}

TEST_CASE_FIXTURE(CliFixture, "cli export --format=csv or json without a list holds both") {
	Seed();
	CHECK(Run({"export", "--format=json"}) == 0);
	CHECK(std::count(out.begin(), out.end(), '\n') == 4);
	CHECK(out.find("{\"ref\":\"a:1\",") == 0);
	CHECK(Has(out, "\n{\"ref\":\"m:1\",\"type\":\"manga\",\"title\":\"Berserk\","));
	CHECK(Run({"export", "--format=csv"}) == 0);
	// The column names, then the four titles.
	CHECK(std::count(out.begin(), out.end(), '\n') == 5);
}

TEST_CASE_FIXTURE(CliFixture, "cli export wants a list and a format it knows") {
	Seed();
	CHECK(Run({"export"}) == 2);
	CHECK(err.find("error: say which list: anime or manga (XML is one a file)\n"
	               "Usage: catalist export ") == 0);
	CHECK(Run({"export", "--format=csv", "anime", "manga"}) == 2);
	CHECK(err.find("error: say which list: anime, manga, or none for both\n") == 0);
	CHECK(Run({"export", "books"}) == 2);
	CHECK(Run({"export", "anime", "manga"}) == 2);
	CHECK(Run({"export", "--format=yaml", "anime"}) == 2);
	CHECK(err.find("error: unknown format 'yaml'") == 0);
	CHECK(Has(err, "; choose xml, csv or json\nUsage: catalist export "));
	CHECK(Run({"export", "--format=jsno", "anime"}) == 2);
	CHECK(err.find("error: unknown format 'jsno' (did you mean 'json'?); choose xml, csv or "
	               "json\n") == 0);
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
	// The other formats leave nothing out.
	CHECK(Run({"export", "--format=csv", "anime"}) == 0);
	CHECK(err.empty());
	CHECK(std::count(out.begin(), out.end(), '\n') == 3);
}

TEST_CASE_FIXTURE(CliFixture, "cli export of an empty catalogue is an empty list") {
	CHECK(Run({"export", "manga"}) == 0);
	CHECK(err.empty());
	CHECK(out.find("<user_export_type>2</user_export_type>") != std::string::npos);
	CHECK(catalist::mal_xml::Parse(out).empty());
	CHECK(Run({"export", "--format=csv", "manga"}) == 0);
	CHECK(out ==
	      "ref,type,title,status,score,progress,total,volumes,rewatched,started,finished,"
	      "updated,new,total_volumes,released,anilist_id,mal_id,url,kitsu_id\n");
	CHECK(Run({"export", "--format=json", "manga"}) == 0);
	CHECK(out.empty());
	CHECK_FALSE(std::filesystem::exists("data"));
}

TEST_CASE_FIXTURE(CliFixture, "cli import --user moves in a user's lists from a site") {
	Seed();
	web.Respond(R"("u":"Abel","t":"ANIME")",
	            R"({"data":{"MediaListCollection":{"lists":[{"entries":[
	            {"status":"COMPLETED","score":9,"progress":13,"repeat":0,"startedAt":{},"completedAt":{},
	             "media":{"id":128547,"idMal":44265,"title":{"romaji":"Odd Taxi"},"episodes":13,"status":"FINISHED"}},
	            {"status":"CURRENT","score":0,"progress":3,"repeat":0,"startedAt":{},"completedAt":{},
	             "media":{"id":171018,"idMal":57334,"title":{"romaji":"Dandadan"},"episodes":12,"status":"FINISHED"}}]}]}}})");
	CHECK(Run({"import", "-a", "--user", "Abel", "--from", "anilist"}) == 0);
	CHECK(Has(err, "Abel on AniList (anime): 2 titles, 1 added, 1 in your lists already\n"));
	auto dandadan = SavedTitle(MediaType::kAnime, "Dandadan");
	CHECK(dandadan.progress == 3);
	CHECK(dandadan.anilist_id == 171018);
	// A title there already keeps what the person gave it.
	CHECK(SavedTitle(MediaType::kAnime, "Odd Taxi").progress == 5);
	CHECK(Steps().size() == 1);

	web.Respond(R"("u":"Nobody")", R"({"errors":[{"message":"Not Found."}]})", 404);
	CHECK(Run({"import", "--user", "Nobody"}) == 1);
	CHECK(err == "error: AniList has no public list of Nobody\n");
	CHECK(Run({"import", "--user", "Abel", "file.xml"}) == 2);
	CHECK(Run({"import", "-a", "file.xml"}) == 2);
	CHECK(err.find("error: --from, -a and -m go with --user\n") == 0);
}

TEST_CASE_FIXTURE(CliFixture, "cli export --format=json is a backup import reads back whole") {
	Seed();
	REQUIRE(Run({"link", "taxi", "kitsu", "43095"}) == 0);
	REQUIRE(Run({"set", "taxi", "rewatched=1", "finish=2026-10-19"}) == 0);
	REQUIRE(Run({"export", "--format=json"}) == 0);
	std::string backup = out;
	Put("backup.jsonl", backup);

	std::filesystem::remove_all("data");
	CHECK(Run({"import", "backup.jsonl"}) == 0);
	CHECK(Has(err, "backup.jsonl: 4 titles, 4 added, 0 in your lists already\n"));
	CHECK(Run({"export", "--format=json"}) == 0);
	CHECK(out == backup);
	CHECK(SavedTitle(MediaType::kAnime, "Odd Taxi").kitsu_id == 43095);
	// Again: all of them are there already.
	CHECK(Run({"import", "-"}, backup) == 0);
	CHECK(Has(err, "standard input: 4 titles, 0 added, 4 in your lists already\n"));

	Put("bad.jsonl", "{\"type\":\"anime\",\"title\":\"x\",\"status\":\"planned\"}\n{\"a\":1}\n");
	CHECK(Run({"import", "bad.jsonl"}) == 1);
	CHECK(err ==
	      "error: line 2 of bad.jsonl is not a title as 'export --format=json' writes one\n");
}
