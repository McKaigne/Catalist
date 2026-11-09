#include <doctest/doctest.h>

#include <iostream>
#include <string>
#include <vector>

#include "catalist/cli.hpp"
#include "catalist/storage/database.hpp"
#include "support/sampleLibrary.hpp"
#include "support/testSupport.hpp"

using catalist::cliDispatcher;
using catalist::mediaType;
using catalist::watchStatus;
using catalist::test::streamCapture;
using catalist::test::tempDirFixture;

namespace {

struct setFixture : tempDirFixture {
  setFixture() : db(fileSystem(), clock(), tempDir()) { catalist::test::seedSampleLibrary(db); }
  catalist::database db;
  std::string out;
  std::string err;

  int set(const std::vector<std::string>& args) {
    streamCapture outCapture(std::cout);
    streamCapture errCapture(std::cerr);
    int exitCode = cliDispatcher::executeSet(args, db, clock());
    out = outCapture.text();
    err = errCapture.text();
    return exitCode;
  }

  catalist::entry anime(int id) const {
    auto found = db.getById(mediaType::kAnime, id);
    REQUIRE(found.has_value());
    return *found;
  }
};

}  // namespace

TEST_CASE_FIXTURE(setFixture, "cli set progress at total completes the entry") {
  CHECK(set({"44265", "-p", "13"}) == 0);
  CHECK(anime(44265).progress == 13);
  CHECK(anime(44265).status == watchStatus::kCompleted);
  CHECK(out == "Updated [44265] 'Odd Taxi': Progress 13/13, Rating 8/10, "
               "Status Completed\n");
}

TEST_CASE_FIXTURE(setFixture, "cli set progress above total clamps and completes") {
  CHECK(set({"44265", "-p", "+50"}) == 0);
  CHECK(anime(44265).progress == 13);
  CHECK(anime(44265).status == watchStatus::kCompleted);
}

TEST_CASE_FIXTURE(setFixture, "cli set explicit status wins over auto completion") {
  CHECK(set({"44265", "-p", "13", "-s", "on-hold"}) == 0);
  CHECK(anime(44265).status == watchStatus::kOnHold);
}

TEST_CASE_FIXTURE(setFixture, "cli set negative delta stops at zero") {
  CHECK(set({"44265", "-p", "sub", "99"}) == 0);
  CHECK(anime(44265).progress == 0);
  CHECK(anime(44265).status == watchStatus::kWatching);

  CHECK(set({"10002", "-p", "-7"}) == 0);
  CHECK(anime(10002).progress == 0);
}

TEST_CASE_FIXTURE(setFixture, "cli set targets by id") {
  CHECK(set({"10002", "-r", "9"}) == 0);
  CHECK(anime(10002).score == 9);
}

TEST_CASE_FIXTURE(setFixture, "cli set targets by exact title case insensitively") {
  CHECK(set({"odd taxi", "-r", "3"}) == 0);
  CHECK(anime(44265).score == 3);
}

TEST_CASE_FIXTURE(setFixture, "cli set targets by unique substring") {
  CHECK(set({"Shell", "-n", "updated"}) == 0);
  CHECK(anime(10002).notes == "updated");
}

TEST_CASE_FIXTURE(setFixture, "cli set falls through to manga") {
  CHECK(set({"berserk", "-p", "a"}) == 0);
  auto berserk = db.getById(mediaType::kManga, 2);
  REQUIRE(berserk.has_value());
  CHECK(berserk->progress == 365);
}

TEST_CASE_FIXTURE(setFixture, "cli set reports an ambiguous substring") {
  CHECK(set({"o", "-r", "1"}) == 1);
  CHECK(out == "");
  CHECK(err == "error: ambiguous query 'o'. Matches multiple entries.\n");
  CHECK(anime(44265).score == 8);
}

TEST_CASE_FIXTURE(setFixture, "cli set reports not found") {
  CHECK(set({"Nonexistent", "-r", "1"}) == 1);
  CHECK(err == "error: entry not found matching 'Nonexistent'.\n");
}

TEST_CASE_FIXTURE(setFixture, "cli set reports not found as json") {
  CHECK(set({"424242", "-r", "1", "--json"}) == 1);
  CHECK(err == "{\"error\":{\"code\":\"notFound\",\"message\":\"entry not "
               "found matching '424242'.\"}}\n");
}

TEST_CASE_FIXTURE(setFixture, "cli set reads targets from stdin") {
  catalist::test::stdinFeed input("44265\n  Ghost, \"in\" the Shell \n\nmissing\n");
  CHECK(set({"-", "-r", "2"}) == 0);
  CHECK(anime(44265).score == 2);
  CHECK(anime(10002).score == 2);
  CHECK(err == "error: entry not found matching 'missing'.\n");
}

TEST_CASE_FIXTURE(setFixture, "cli set with empty stdin is an input error") {
  catalist::test::stdinFeed input("\n\n");
  CHECK(set({"-", "-r", "2"}) == 1);
  CHECK(err == "error: no target identifiers received from stdin.\n");
}
