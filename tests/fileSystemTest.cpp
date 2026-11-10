#include <doctest/doctest.h>

#include <filesystem>

#include "catalist/platform/fileSystem.hpp"
#include "support/testSupport.hpp"

using catalist::realFileSystem;
using catalist::test::tempDirFixture;

TEST_CASE_FIXTURE(tempDirFixture,
                  "realFileSystem writes atomically into new directories") {
  realFileSystem fs;
  auto target = tempDir() / "nested" / "file.txt";
  REQUIRE(fs.writeFileAtomic(target, "content"));
  CHECK(fs.readFile(target) == "content");
  CHECK_FALSE(std::filesystem::exists(target.string() + ".tmp"));
  REQUIRE(fs.writeFileAtomic(target, "replaced"));
  CHECK(fs.readFile(target) == "replaced");
}

TEST_CASE_FIXTURE(tempDirFixture,
                  "realFileSystem reports missing files and failed writes") {
  realFileSystem fs;
  CHECK_FALSE(fs.readFile(tempDir() / "missing.txt").has_value());
  // A directory stands where the file should go.
  std::filesystem::create_directories(tempDir() / "taken");
  CHECK_FALSE(fs.writeFileAtomic(tempDir() / "taken", "x"));
  CHECK_FALSE(std::filesystem::exists(tempDir() / "taken.tmp"));
}
