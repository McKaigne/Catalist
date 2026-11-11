#include <doctest/doctest.h>

#include <filesystem>

#include "support/testSupport.hpp"
#include "system.hpp"

using catalist::test::tempDirFixture;

TEST_CASE_FIXTURE(
    tempDirFixture,
    "writeFileAtomic writes into new directories and leaves no temp file") {
  auto target = tempDir() / "nested" / "file.txt";
  REQUIRE(catalist::writeFileAtomic(target, "content"));
  CHECK(catalist::readFile(target) == "content");
  CHECK_FALSE(std::filesystem::exists(target.string() + ".tmp"));
  REQUIRE(catalist::writeFileAtomic(target, "replaced"));
  CHECK(catalist::readFile(target) == "replaced");
}

TEST_CASE_FIXTURE(
    tempDirFixture,
    "readFile and writeFileAtomic report missing files and failed writes") {
  CHECK_FALSE(catalist::readFile(tempDir() / "missing.txt").has_value());
  // A directory stands where the file should go.
  std::filesystem::create_directories(tempDir() / "taken");
  CHECK_FALSE(catalist::writeFileAtomic(tempDir() / "taken", "x"));
  // Reading a directory used to throw instead of reporting "no file".
  CHECK_FALSE(catalist::readFile(tempDir() / "taken").has_value());
  CHECK_FALSE(std::filesystem::exists(tempDir() / "taken.tmp"));
}

using catalist::shellQuote;

TEST_CASE("shellQuote wraps plain text in single quotes") {
  CHECK(shellQuote("hello world") == "'hello world'");
  CHECK(shellQuote("") == "''");
}

TEST_CASE("shellQuote neutralises quotes and substitutions") {
  CHECK(shellQuote("it's") == "'it'\\''s'");
  CHECK(shellQuote("$(rm -rf ~); `x` \"y\"") == "'$(rm -rf ~); `x` \"y\"'");
}

TEST_CASE("today follows the computer's calendar") {
  // The test main pins TZ=UTC; the date must be a real one, not a default.
  auto date = catalist::today();
  CHECK(date.year() >= 2024);
  CHECK(catalist::isoDate::parse(date.toString()) == date);
}
