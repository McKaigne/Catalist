#include <doctest/doctest.h>

#include <algorithm>
#include <filesystem>
#include <string>

#include "storage/malXml.hpp"
#include "support/cliFixture.hpp"

using catalist::mediaType;
using catalist::watchStatus;
using catalist::test::cliFixture;
using catalist::test::has;
using catalist::test::kAnimeXml;
using catalist::test::kMangaXml;
using catalist::test::makeEntry;

TEST_CASE_FIXTURE(cliFixture, "cli import adds the titles and saves them") {
	put("lists/anime.xml", kAnimeXml);
	put("lists/manga.xml", kMangaXml);

	CHECK(run({"import", "lists/anime.xml", "lists/manga.xml"}) == 0);
	CHECK(out ==
	      "a:1\tSousou no Frieren\tadded\t-\tcompleted\n"
	      "a:2\tOdd Taxi\tadded\t-\twatching\n"
	      "m:1\tBerserk\tadded\t-\twatching\n");
	CHECK(err ==
	      "lists/anime.xml: 2 titles, 2 added, 0 in your lists already\n"
	      "lists/manga.xml: 1 title, 1 added, 0 in your lists already\n");

	auto anime = saved(mediaType::kAnime);
	REQUIRE(anime.size() == 2);
	CHECK(anime[0].title == "Sousou no Frieren");
	CHECK(anime[0].malId == 52991);
	CHECK(anime[0].score == 10);
	CHECK(anime[0].progress == 28);
	CHECK(anime[0].status == catalist::watchStatus::kCompleted);
	CHECK(anime[1].title == "Odd Taxi");
	auto manga = saved(mediaType::kManga);
	REQUIRE(manga.size() == 1);
	CHECK(manga[0].title == "Berserk");
	CHECK(manga[0].progress == 364);

	// All the files of one command are one step for undo.
	REQUIRE(steps().size() == 1);
	CHECK(steps()[0].label == "import lists/anime.xml lists/manga.xml");
	CHECK(steps()[0].changes == 3);
	CHECK(run({"undo", "-q"}) == 0);
	CHECK(saved(mediaType::kAnime).empty());
	CHECK(saved(mediaType::kManga).empty());
}

TEST_CASE_FIXTURE(cliFixture, "cli import reads standard input for a FILE of -") {
	CHECK(run({"import", "-"}, kAnimeXml) == 0);
	CHECK(has(out, "\tOdd Taxi\tadded\t-\twatching\n"));
	CHECK(err == "standard input: 2 titles, 2 added, 0 in your lists already\n");
	CHECK(saved(mediaType::kAnime).size() == 2);
	// -q prints nothing, not even that.
	CHECK(run({"import", "-q", "-"}, kAnimeXml) == 0);
	CHECK(out.empty());
	CHECK(err.empty());
	CHECK(run({"import", "-"}, "not an export") == 1);
	CHECK(err ==
	      "error: no anime or manga found in standard input (is it a MyAnimeList XML export?)\n");
	CHECK(run({"import", "-"}, std::string("\x1f\x8b\x08\x00", 4) + "gz") == 1);
	CHECK(err == "error: standard input is compressed; unpack it first (gunzip)\n");
}

TEST_CASE_FIXTURE(cliFixture, "cli import adds only the titles not tracked yet") {
	seed();
	put("lists/anime.xml", kAnimeXml);
	// Both titles are there already.
	CHECK(run({"import", "lists/anime.xml"}) == 0);
	CHECK(out.empty());
	CHECK(has(err, "2 titles, 0 added, 2 in your lists already\n"));
	put("lists/manga.xml",
	    "<myanimelist><manga><manga_mangadb_id>656</manga_mangadb_id>"
	    "<manga_title><![CDATA[Vagabond]]></manga_title>"
	    "<my_status>Plan to Read</my_status></manga></myanimelist>");
	CHECK(run({"import", "lists/manga.xml"}) == 0);
	CHECK(out == "m:3\tVagabond\tadded\t-\tplanned\n");
}

TEST_CASE_FIXTURE(cliFixture, "cli importing again keeps what the user changed") {
	put("lists/anime.xml", kAnimeXml);
	REQUIRE(run({"import", "lists/anime.xml"}) == 0);

	{
		catalist::catalogue db("data");
		REQUIRE(db.loadAll().ok());
		auto taxi = db.getEntries(mediaType::kAnime)[1];
		REQUIRE(taxi.title == "Odd Taxi");
		taxi.score = 9;
		taxi.progress = 13;
		taxi.startDate = catalist::test::requireDate("2026-01-02");
		REQUIRE(db.updateEntry(taxi));
		REQUIRE(db.saveAll());
	}

	CHECK(run({"import", "lists/anime.xml"}) == 0);
	CHECK(out.empty());
	CHECK(err ==
	      "lists/anime.xml: 2 titles, 0 added, 2 in your lists already\n"
	      "Nothing changed.\n");
	auto anime = saved(mediaType::kAnime);
	REQUIRE(anime.size() == 2);
	CHECK(anime[1].score == 9);
	CHECK(anime[1].progress == 13);
	CHECK(catalist::formatDate(anime[1].startDate) == "2026-01-02");
	CHECK(steps().size() == 1);
}

TEST_CASE_FIXTURE(cliFixture, "cli import changes nothing when any file is bad") {
	put("lists/anime.xml", kAnimeXml);
	put("lists/empty.xml", "<myanimelist></myanimelist>");

	CHECK(run({"import", "lists/anime.xml", "lists/missing.xml"}) == 1);
	CHECK(err == "error: cannot read lists/missing.xml\n");
	CHECK(out.empty());
	CHECK(saved(mediaType::kAnime).empty());

	put("lists/anime.xml.gz", std::string("\x1f\x8b\x08\x00", 4) + "gz");
	CHECK(run({"import", "lists/anime.xml.gz"}) == 1);
	CHECK(err ==
	      "error: lists/anime.xml.gz is compressed; unpack it first "
	      "(gunzip lists/anime.xml.gz)\n");

	CHECK(run({"import", "lists/anime.xml", "lists/empty.xml"}) == 1);
	CHECK(err ==
	      "error: no anime or manga found in lists/empty.xml (is it a "
	      "MyAnimeList XML export?)\n");
	CHECK(saved(mediaType::kAnime).empty());
}

TEST_CASE_FIXTURE(cliFixture, "cli import exits 1 when the catalogue cannot be saved") {
	put("lists/anime.xml", kAnimeXml);
	catalist::test::makeUnwritable("data/catalist.db");
	CHECK(run({"import", "lists/anime.xml"}) == 1);
	CHECK(err.find("error: cannot open data/catalist.db: ") == 0);
	CHECK(run({"add", "--no-lookup", "Home Video"}) == 1);
	CHECK(err.find("error: cannot open data/catalist.db: ") == 0);
	CHECK(out.empty());
}

TEST_CASE_FIXTURE(cliFixture, "cli export prints what import read") {
	put("lists/anime.xml", kAnimeXml);
	put("lists/manga.xml", kMangaXml);
	REQUIRE(run({"import", "lists/anime.xml", "lists/manga.xml"}) == 0);

	CHECK(run({"export", "anime"}) == 0);
	CHECK(err.empty());
	auto anime = catalist::malXml::parse(out);
	REQUIRE(anime.size() == 2);
	CHECK(anime[0].title == "Sousou no Frieren");
	CHECK(anime[0].malId == 52991);
	CHECK(anime[0].progress == 28);
	CHECK(anime[1].title == "Odd Taxi");
	CHECK(anime[1].score == 8);
	// xml is the format when none is named.
	std::string xml = out;
	CHECK(run({"export", "--format=xml", "anime"}) == 0);
	CHECK(out == xml);

	CHECK(run({"export", "manga"}) == 0);
	auto manga = catalist::malXml::parse(out);
	REQUIRE(manga.size() == 1);
	CHECK(manga[0].type == mediaType::kManga);
	CHECK(manga[0].title == "Berserk");
}

TEST_CASE_FIXTURE(cliFixture, "cli export --format=csv prints the columns of ls, quoted as CSV") {
	seed();
	REQUIRE(run({"add", "--no-lookup", "Say \"hi\", twice"}) == 0);
	CHECK(run({"export", "--format=csv", "anime"}) == 0);
	CHECK(err.empty());
	CHECK(out ==
	      "ref,type,title,status,score,progress,total,volumes,rewatched,started,finished,"
	      "updated,new\n"
	      "a:1,anime,Sousou no Frieren,completed,10,28,28,0,0,,,,0\n"
	      "a:2,anime,Odd Taxi,watching,8,5,13,0,0,2026-10-01,,,8\n"
	      "a:3,anime,\"Say \"\"hi\"\", twice\",planned,0,0,,0,0,,,2026-10-20,0\n");
	// The same at a terminal: a file is what it is for.
	std::string csv = out;
	CHECK(run({"export", "--format", "csv", "anime"}, "", 100) == 0);
	CHECK(out == csv);
	CHECK(run({"export", "--format=csv", "manga"}) == 0);
	CHECK(std::count(out.begin(), out.end(), '\n') == 3);
}

TEST_CASE_FIXTURE(cliFixture, "cli export --format=json prints one object a title") {
	seed();
	CHECK(run({"export", "--format=json", "anime"}) == 0);
	CHECK(std::count(out.begin(), out.end(), '\n') == 2);
	CHECK(out.find("{\"ref\":\"a:1\",\"type\":\"anime\",\"title\":\"Sousou no Frieren\",") == 0);
	CHECK(
	    has(out,
	        "\n{\"ref\":\"a:2\",\"type\":\"anime\",\"title\":\"Odd Taxi\",\"status\":\"watching\","
	        "\"score\":8,\"progress\":5,\"total\":13,"));
	// The lines of `ls --json`.
	std::string json = out;
	CHECK(run({"ls", "-a", "--sort=updated", "--json"}) == 0);
	CHECK(out.size() == json.size());
}

TEST_CASE_FIXTURE(cliFixture, "cli export --format=csv or json without a list holds both") {
	seed();
	CHECK(run({"export", "--format=json"}) == 0);
	CHECK(std::count(out.begin(), out.end(), '\n') == 4);
	CHECK(out.find("{\"ref\":\"a:1\",") == 0);
	CHECK(has(out, "\n{\"ref\":\"m:1\",\"type\":\"manga\",\"title\":\"Berserk\","));
	CHECK(run({"export", "--format=csv"}) == 0);
	// The column names, then the four titles.
	CHECK(std::count(out.begin(), out.end(), '\n') == 5);
}

TEST_CASE_FIXTURE(cliFixture, "cli export wants a list and a format it knows") {
	seed();
	CHECK(run({"export"}) == 2);
	CHECK(err.find("error: say which list: anime or manga (XML is one a file)\n"
	               "Usage: catalist export ") == 0);
	CHECK(run({"export", "--format=csv", "anime", "manga"}) == 2);
	CHECK(err.find("error: say which list: anime, manga, or none for both\n") == 0);
	CHECK(run({"export", "books"}) == 2);
	CHECK(run({"export", "anime", "manga"}) == 2);
	CHECK(run({"export", "--format=yaml", "anime"}) == 2);
	CHECK(err.find("error: unknown format 'yaml'") == 0);
	CHECK(has(err, "; choose xml, csv or json\nUsage: catalist export "));
	CHECK(run({"export", "--format=jsno", "anime"}) == 2);
	CHECK(err.find("error: unknown format 'jsno' (did you mean 'json'?); choose xml, csv or "
	               "json\n") == 0);
	CHECK(out.empty());
}

TEST_CASE_FIXTURE(cliFixture, "cli export notes the titles it had to leave out") {
	{
		catalist::catalogue db("data");
		db.addEntry(catalist::test::makeEntry().title("Home video"));
		db.addEntry(catalist::test::makeEntry().title("Another one"));
		REQUIRE(db.saveAll());
	}
	CHECK(run({"export", "anime"}) == 0);
	CHECK(err == "note: 2 titles without a MyAnimeList id were left out.\n");
	CHECK(catalist::malXml::parse(out).empty());
	CHECK(out.find("<myanimelist>") != std::string::npos);
	// The other formats leave nothing out.
	CHECK(run({"export", "--format=csv", "anime"}) == 0);
	CHECK(err.empty());
	CHECK(std::count(out.begin(), out.end(), '\n') == 3);
}

TEST_CASE_FIXTURE(cliFixture, "cli export of an empty catalogue is an empty list") {
	CHECK(run({"export", "manga"}) == 0);
	CHECK(err.empty());
	CHECK(out.find("<user_export_type>2</user_export_type>") != std::string::npos);
	CHECK(catalist::malXml::parse(out).empty());
	CHECK(run({"export", "--format=csv", "manga"}) == 0);
	CHECK(out ==
	      "ref,type,title,status,score,progress,total,volumes,rewatched,started,finished,"
	      "updated,new\n");
	CHECK(run({"export", "--format=json", "manga"}) == 0);
	CHECK(out.empty());
	CHECK_FALSE(std::filesystem::exists("data"));
}

TEST_CASE_FIXTURE(cliFixture, "cli import --user moves in a user's lists from a site") {
	seed();
	web.respond(R"("u":"Abel","t":"ANIME")",
	            R"({"data":{"MediaListCollection":{"lists":[{"entries":[
	            {"status":"COMPLETED","score":9,"progress":13,"repeat":0,"startedAt":{},"completedAt":{},
	             "media":{"id":128547,"idMal":44265,"title":{"romaji":"Odd Taxi"},"episodes":13,"status":"FINISHED"}},
	            {"status":"CURRENT","score":0,"progress":3,"repeat":0,"startedAt":{},"completedAt":{},
	             "media":{"id":171018,"idMal":57334,"title":{"romaji":"Dandadan"},"episodes":12,"status":"FINISHED"}}]}]}}})");
	CHECK(run({"import", "-a", "--user", "Abel", "--from", "anilist"}) == 0);
	CHECK(has(err, "Abel on AniList (anime): 2 titles, 1 added, 1 in your lists already\n"));
	auto dandadan = savedTitle(mediaType::kAnime, "Dandadan");
	CHECK(dandadan.progress == 3);
	CHECK(dandadan.anilistId == 171018);
	// A title there already keeps what the person gave it.
	CHECK(savedTitle(mediaType::kAnime, "Odd Taxi").progress == 5);
	CHECK(steps().size() == 1);

	CHECK(run({"import", "--user", "Nobody"}) == 1);
	CHECK(err == "error: could not reach AniList, or it has no public list of Nobody; check the "
	             "connection and try again\n");
	CHECK(run({"import", "--user", "Abel", "file.xml"}) == 2);
	CHECK(run({"import", "-a", "file.xml"}) == 2);
	CHECK(err.find("error: --from, -a and -m go with --user\n") == 0);
}
