#include <doctest/doctest.h>

#include <iostream>
#include <string>
#include <vector>

#include "catalist/cli/commands/commands.hpp"
#include "catalist/storage/database.hpp"
#include "support/sampleLibrary.hpp"
#include "support/testSupport.hpp"

using catalist::mediaType;
using catalist::test::streamCapture;
using catalist::test::tempDirFixture;

namespace {

struct commandFixture : tempDirFixture {
  commandFixture()
      : db(fileSystem(), clock(), tempDir()),
        harness(db, fileSystem(), clock(), tempDir()) {
    catalist::test::seedSampleLibrary(db);
  }

  template <typename handler>
  int run(handler command, const std::vector<std::string>& args) {
    streamCapture outCapture(std::cout);
    streamCapture errCapture(std::cerr);
    int exitCode = command(args, harness.context);
    out = outCapture.text();
    err = errCapture.text();
    return exitCode;
  }

  catalist::database db;
  catalist::test::cliHarness harness;
  std::string out;
  std::string err;
};

const char* kAnilistBody =
    R"({"data":{"Page":{"media":[)"
    R"({"id":154587,"idMal":52991,"title":{"romaji":"Sousou no Frieren","english":"Frieren"},"episodes":28,"averageScore":91,"startDate":{"year":2023}}]}}})";

}  // namespace

TEST_CASE_FIXTURE(commandFixture, "cli random uses the injected picker") {
  CHECK(run(catalist::cli::runRandom, {"--plain", "--sort", "title"}) == 0);
  CHECK(out == "A Very Long Title That Will Definitely Be Truncated\n");
  CHECK(run(catalist::cli::runRandom, {"--ids", "-t", "manga", "--sort", "id"}) == 0);
  CHECK(out == "2\n");
}

TEST_CASE_FIXTURE(commandFixture, "cli random reports when nothing matches") {
  CHECK(run(catalist::cli::runRandom, {"-q", "no such title"}) == 1);
  CHECK(err == "error: no entries match the specified filters.\n");
}

TEST_CASE_FIXTURE(commandFixture, "cli count applies rating and year filters") {
  CHECK(run(catalist::cli::runCount, {"-r", ">=9"}) == 0);
  CHECK(out == "3\n");
  CHECK(run(catalist::cli::runCount, {"--year", "2021", "--json"}) == 0);
  CHECK(out == "{\"count\":1}\n");
  CHECK(run(catalist::cli::runCount, {"--unrated"}) == 0);
  CHECK(out == "1\n");
}

TEST_CASE_FIXTURE(commandFixture, "cli count rejects bad flags with exit code 2") {
  CHECK(run(catalist::cli::runCount, {"--limit", "-3"}) == 2);
  CHECK(err == "error: --limit must be non-negative\n");
  CHECK(run(catalist::cli::runCount, {"--json", "--sort", "colour"}) == 2);
  CHECK(err.find("\"code\":\"usage\"") != std::string::npos);
}

TEST_CASE_FIXTURE(commandFixture, "cli show reads several targets from stdin") {
  catalist::test::stdinFeed input("2\nOdd Taxi\n");
  CHECK(run(catalist::cli::runShow, {"-", "--json"}) == 0);
  CHECK(out.find("\"title\":\"Berserk\"") != std::string::npos);
  CHECK(out.find("\"title\":\"Odd Taxi\"") != std::string::npos);
}

TEST_CASE_FIXTURE(commandFixture, "cli search prints provider results without prompting off a tty") {
  harness.http.setResponse("graphql.anilist.co", {200, kAnilistBody, ""});
  CHECK(run(catalist::cli::runSearch, {"frieren", "--json"}) == 0);
  CHECK(out.find("\"malId\":52991") != std::string::npos);
  CHECK(out.find("\"title\":\"Sousou no Frieren\"") != std::string::npos);
  REQUIRE(harness.http.requests().size() == 1);
  CHECK(harness.http.requests()[0].method == "POST");
}

TEST_CASE_FIXTURE(commandFixture, "cli search reports failure when providers are unreachable") {
  CHECK(run(catalist::cli::runSearch, {"nothing"}) == 1);
  CHECK(err.find("Failed to contact remote providers") != std::string::npos);
}

TEST_CASE_FIXTURE(commandFixture, "cli import ingests an xml export") {
  auto xmlPath = tempDir() / "export.xml";
  catalist::test::writeTextFile(
      xmlPath,
      "<anime><series_animedb_id>7</series_animedb_id>"
      "<series_title>Imported</series_title><my_status>Completed</my_status>"
      "</anime>\n");
  CHECK(run(catalist::cli::runImport, {xmlPath.string()}) == 0);
  CHECK(out == "Successfully ingested 1 records from: " + xmlPath.string() +
                   "\nImport complete. 1 total records loaded.\n");
  CHECK(db.getById(mediaType::kAnime, 7).has_value());
}

TEST_CASE_FIXTURE(commandFixture, "cli import needs a file") {
  CHECK(run(catalist::cli::runImport, {"--clean"}) == 2);
  CHECK(err == "error: no XML files specified for import.\n");
}

TEST_CASE_FIXTURE(commandFixture, "cli config set persists and get reads it back") {
  CHECK(run(catalist::cli::runConfig, {"set", "theme", "nord"}) == 0);
  CHECK(out == "Set theme = nord\n");
  CHECK(catalist::loadSettings(fileSystem(), tempDir() / "settings.json").theme ==
        "nord");
  CHECK(run(catalist::cli::runConfig, {"get", "theme"}) == 0);
  CHECK(out == "nord\n");
  CHECK(run(catalist::cli::runConfig, {"get", "colour"}) == 1);
}

TEST_CASE_FIXTURE(commandFixture, "cli config set rejects values outside the choices") {
  CHECK(run(catalist::cli::runConfig, {"set", "default_media", "novels"}) == 1);
  CHECK(err == "error: invalid value 'novels' for default_media.\n");
  CHECK(run(catalist::cli::runConfig, {"set", "startup_tab", "stats"}) == 0);
  CHECK(run(catalist::cli::runConfig, {"get", "startup_tab"}) == 0);
  CHECK(out == "stats\n");
  CHECK_FALSE(fileSystem().exists(tempDir() / "catalist.kdl"));
}

TEST_CASE_FIXTURE(commandFixture, "cli rm refuses to delete without --yes off a tty") {
  CHECK(run(catalist::cli::runRm, {"44265"}) == 1);
  CHECK(err.find("requires '--yes'") != std::string::npos);
  CHECK(db.getById(mediaType::kAnime, 44265).has_value());
}

TEST_CASE_FIXTURE(commandFixture, "cli stats text output lists both media") {
  CHECK(run(catalist::cli::runStats, {}) == 0);
  CHECK(out.find("=== Anime Telemetry ===") != std::string::npos);
  CHECK(out.find("Volumes Logged:     53") != std::string::npos);
}

TEST_CASE_FIXTURE(commandFixture, "cli completion rejects unknown shells") {
  CHECK(run(catalist::cli::runCompletion, {"tcsh"}) != 0);
}
