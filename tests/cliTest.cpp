#include <doctest/doctest.h>

#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#include "cli.hpp"
#include "malXml.hpp"
#include "support/testSupport.hpp"
#include "version.hpp"

namespace {

using catalist::mediaType;

const char* const kAnimeXml =
    "<myanimelist>"
    "<anime><series_animedb_id>52991</series_animedb_id>"
    "<series_title><![CDATA[Sousou no Frieren]]></series_title>"
    "<series_episodes>28</series_episodes>"
    "<my_watched_episodes>28</my_watched_episodes><my_score>10</my_score>"
    "<my_status>Completed</my_status></anime>"
    "<anime><series_animedb_id>44265</series_animedb_id>"
    "<series_title><![CDATA[Odd Taxi]]></series_title>"
    "<series_episodes>13</series_episodes>"
    "<my_watched_episodes>5</my_watched_episodes><my_score>8</my_score>"
    "<my_status>Watching</my_status></anime>"
    "</myanimelist>";

const char* const kMangaXml =
    "<myanimelist>"
    "<manga><manga_mangadb_id>2</manga_mangadb_id>"
    "<manga_title><![CDATA[Berserk]]></manga_title>"
    "<my_read_chapters>364</my_read_chapters><my_score>10</my_score>"
    "<my_status>Reading</my_status></manga>"
    "</myanimelist>";

// One `catalist ...` run with the data in data/ of a temporary directory,
// which is the working directory while the test runs, so file names in the
// output stay short. Each run gets a fresh catalogue, as each real
// invocation does.
struct cliFixture : catalist::test::tempDirFixture {
  cliFixture() : previousDir(std::filesystem::current_path()) {
    std::filesystem::current_path(tempDir());
  }
  ~cliFixture() { std::filesystem::current_path(previousDir); }

  int run(const std::vector<std::string>& args) {
    catalist::catalogue db("data");
    std::ostringstream outStream;
    std::ostringstream errStream;
    int exitCode = catalist::runCli(args, db, outStream, errStream);
    out = outStream.str();
    err = errStream.str();
    return exitCode;
  }

  std::vector<catalist::entry> saved(mediaType type) {
    catalist::catalogue db("data");
    REQUIRE(db.loadAll().ok());
    return db.getEntries(type);
  }

  void put(const std::string& path, const std::string& content) {
    catalist::test::writeText(path, content);
  }

  std::filesystem::path previousDir;
  std::string out;
  std::string err;
};

}  // namespace

TEST_CASE_FIXTURE(cliFixture, "cli --version prints the version") {
  CHECK(run({"--version"}) == 0);
  CHECK(out == std::string("Catalist v") + catalist::kVersionString + "\n");
  CHECK(err.empty());
  CHECK(run({"-v"}) == 0);
  CHECK(out == std::string("Catalist v") + catalist::kVersionString + "\n");
}

TEST_CASE_FIXTURE(cliFixture, "cli --help lists the commands and data dir") {
  for (const char* flag : {"--help", "-h"}) {
    CHECK(run({flag}) == 0);
    CHECK(out.find("catalist import <file.xml>...") != std::string::npos);
    CHECK(out.find("catalist export <anime|manga>") != std::string::npos);
    CHECK(out.find("The catalogue is kept in data.") != std::string::npos);
    CHECK(err.empty());
  }
}

TEST_CASE_FIXTURE(cliFixture,
                  "cli help and version work with a broken catalogue") {
  put("data/anime.csv", "this,is,not,a,catalogue\n");
  CHECK(run({"--version"}) == 0);
  CHECK(run({"--help"}) == 0);
}

TEST_CASE_FIXTURE(cliFixture,
                  "cli rejects a wrong command line with exit code 2") {
  CHECK(run({"sync"}) == 2);
  CHECK(err.find("error: unknown command 'sync'") == 0);
  CHECK(err.find("Usage:") != std::string::npos);
  CHECK(out.empty());

  CHECK(run({"import"}) == 2);
  CHECK(err.find("error: import needs at least one file") == 0);

  CHECK(run({"export"}) == 2);
  CHECK(err.find("error: export needs one of: anime, manga") == 0);
  CHECK(run({"export", "books"}) == 2);
  CHECK(run({"export", "anime", "manga"}) == 2);
  CHECK(out.empty());
}

TEST_CASE_FIXTURE(cliFixture, "cli import adds the titles and saves them") {
  put("lists/anime.xml", kAnimeXml);
  put("lists/manga.xml", kMangaXml);

  CHECK(run({"import", "lists/anime.xml", "lists/manga.xml"}) == 0);
  CHECK(out ==
        "lists/anime.xml: 2 titles, 2 added, 0 already in the catalogue\n"
        "lists/manga.xml: 1 titles, 1 added, 0 already in the catalogue\n");
  CHECK(err.empty());

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
}

TEST_CASE_FIXTURE(cliFixture,
                  "cli importing again keeps what the user changed") {
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
  CHECK(out ==
        "lists/anime.xml: 2 titles, 0 added, 2 already in the catalogue\n");
  auto anime = saved(mediaType::kAnime);
  REQUIRE(anime.size() == 2);
  CHECK(anime[1].score == 9);
  CHECK(anime[1].progress == 13);
  CHECK(catalist::formatDate(anime[1].startDate) == "2026-01-02");
}

TEST_CASE_FIXTURE(cliFixture,
                  "cli import changes nothing when any file is bad") {
  put("lists/anime.xml", kAnimeXml);
  put("lists/empty.xml", "<myanimelist></myanimelist>");

  CHECK(run({"import", "lists/anime.xml", "lists/missing.xml"}) == 1);
  CHECK(err == "error: cannot read lists/missing.xml\n");
  CHECK(out.empty());
  CHECK(catalist::test::readText("data/anime.csv").empty());

  put("lists/anime.xml.gz", std::string("\x1f\x8b\x08\x00", 4) + "gz");
  CHECK(run({"import", "lists/anime.xml.gz"}) == 1);
  CHECK(err ==
        "error: lists/anime.xml.gz is compressed; unpack it first "
        "(gunzip lists/anime.xml.gz)\n");

  CHECK(run({"import", "lists/anime.xml", "lists/empty.xml"}) == 1);
  CHECK(err ==
        "error: no anime or manga found in lists/empty.xml (is it a "
        "MyAnimeList XML export?)\n");
  CHECK(catalist::test::readText("data/anime.csv").empty());
}

TEST_CASE_FIXTURE(cliFixture,
                  "cli import exits 1 when the catalogue cannot be saved") {
  put("lists/anime.xml", kAnimeXml);
  catalist::test::makeUnwritable("data/anime.csv");
  CHECK(run({"import", "lists/anime.xml"}) == 1);
  CHECK(err == "error: could not save the catalogue to data\n");
}

TEST_CASE_FIXTURE(cliFixture,
                  "cli refuses to work on a catalogue it cannot read") {
  put("data/anime.csv", "this,is,not,a,catalogue\n");
  put("lists/anime.xml", kAnimeXml);
  CHECK(run({"import", "lists/anime.xml"}) == 1);
  CHECK(err.find("error: data/anime.csv: ") == 0);
  CHECK(run({"export", "anime"}) == 1);
  CHECK(out.empty());
  CHECK(catalist::test::readText("data/anime.csv") ==
        "this,is,not,a,catalogue\n");
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

  CHECK(run({"export", "manga"}) == 0);
  auto manga = catalist::malXml::parse(out);
  REQUIRE(manga.size() == 1);
  CHECK(manga[0].type == mediaType::kManga);
  CHECK(manga[0].title == "Berserk");
}

TEST_CASE_FIXTURE(cliFixture,
                  "cli export notes the titles it had to leave out") {
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
}

TEST_CASE_FIXTURE(cliFixture,
                  "cli export of an empty catalogue is an empty list") {
  CHECK(run({"export", "manga"}) == 0);
  CHECK(err.empty());
  CHECK(out.find("<user_export_type>2</user_export_type>") !=
        std::string::npos);
  CHECK(catalist::malXml::parse(out).empty());
}
