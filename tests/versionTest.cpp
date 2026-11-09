#include <doctest/doctest.h>

#include <iostream>
#include <sstream>
#include <string>

#include "catalist/cli.hpp"
#include "catalist/config.hpp"
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
  catalist::appConfig config;
  catalist::test::streamCapture out(std::cout);
  char program[] = "catalist";
  char flag[] = "--version";
  char* argv[] = {program, flag};
  catalist::mockHttpClient http;
  catalist::cliServices services{clock(), http, fileSystem()};
  CHECK(catalist::cliDispatcher::dispatch(2, argv, db, config, services) == 0);
  CHECK(out.text() ==
        std::string("Catalist v") + catalist::kVersionString + " (C++17 / FTXUI)\n");
}
