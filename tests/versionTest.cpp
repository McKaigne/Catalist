#include <doctest/doctest.h>

#include <iostream>
#include <sstream>
#include <string>

#include "catalist/cli/dispatcher.hpp"
#include "catalist/settings/settings.hpp"
#include "catalist/core/version.hpp"
#include "catalist/storage/database.hpp"
#include "support/testSupport.hpp"

namespace {

bool isUnsignedNumber(const std::string& text) {
  if (text.empty())
    return false;
  for (char c : text) {
    if (c < '0' || c > '9')
      return false;
  }
  return true;
}

}  // namespace

TEST_CASE("kVersionString is major.minor.patch matching the numeric parts") {
  std::string version = catalist::kVersionString;
  std::stringstream parts(version);
  std::string major;
  std::string minor;
  std::string patch;
  std::string extra;
  REQUIRE(std::getline(parts, major, '.'));
  REQUIRE(std::getline(parts, minor, '.'));
  REQUIRE(std::getline(parts, patch, '.'));
  CHECK_FALSE(std::getline(parts, extra, '.'));

  REQUIRE(isUnsignedNumber(major));
  REQUIRE(isUnsignedNumber(minor));
  REQUIRE(isUnsignedNumber(patch));
  CHECK(std::stoi(major) == catalist::kVersionMajor);
  CHECK(std::stoi(minor) == catalist::kVersionMinor);
  CHECK(std::stoi(patch) == catalist::kVersionPatch);
}

TEST_CASE_FIXTURE(catalist::test::tempDirFixture, "cli --version prints kVersionString") {
  catalist::database db(fileSystem(), clock(), tempDir());
  catalist::test::streamCapture out(std::cout);
  char program[] = "catalist";
  char flag[] = "--version";
  char* argv[] = {program, flag};
  catalist::test::cliHarness harness(db, fileSystem(), clock(), tempDir());
  CHECK(catalist::cli::dispatch(2, argv, harness.context) == 0);
  CHECK(out.text() ==
        std::string("Catalist v") + catalist::kVersionString + " (C++17 / FTXUI)\n");
}

TEST_CASE("cli write command exits 1 when the catalogue cannot be saved") {
  catalist::memoryFileSystem fs;
  auto clock = catalist::test::makeTestClock();
  catalist::database db(fs, clock, "/data");
  catalist::test::cliHarness harness(db, fs, clock, "/data");
  fs.failWritesTo("/data/anime.csv");
  catalist::test::streamCapture out(std::cout);
  catalist::test::streamCapture err(std::cerr);
  char program[] = "catalist";
  char command[] = "add";
  char title[] = "Frieren";
  char json[] = "--json";
  char* argv[] = {program, command, title, json};
  CHECK(catalist::cli::dispatch(4, argv, harness.context) == 1);
  CHECK(err.text() ==
        "{\"error\":{\"code\":\"io\",\"message\":\"could not save the "
        "catalogue to /data.\"}}\n");
  CHECK_FALSE(db.takeSaveFailure());
}
