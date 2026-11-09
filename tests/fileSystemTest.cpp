#include <doctest/doctest.h>

#include "catalist/platform/fileSystem.hpp"
#include "support/testSupport.hpp"

using catalist::memoryFileSystem;
using catalist::realFileSystem;
using catalist::test::tempDirFixture;

TEST_CASE("memoryFileSystem stores reads appends and removes files") {
  memoryFileSystem fs;
  CHECK_FALSE(fs.readFile("/data/a.csv").has_value());
  CHECK(fs.writeFileAtomic("/data/a.csv", "one"));
  CHECK(fs.appendFile("/data/a.csv", "+two"));
  CHECK(fs.readFile("/data/./a.csv") == "one+two");
  CHECK(fs.exists("/data/a.csv"));
  CHECK(fs.copyFile("/data/a.csv", "/data/b.csv"));
  CHECK(fs.listDirectory("/data").size() == 2);
  CHECK(fs.removeFile("/data/a.csv"));
  CHECK_FALSE(fs.exists("/data/a.csv"));
}

TEST_CASE("memoryFileSystem can simulate write failures and permissions") {
  memoryFileSystem fs;
  fs.failWritesTo("/locked");
  CHECK_FALSE(fs.writeFileAtomic("/locked", "x"));
  CHECK(fs.writeFileAtomic("/secret", "x"));
  CHECK(fs.restrictToOwner("/secret"));
  CHECK(fs.isOwnerOnly("/secret"));
}

TEST_CASE_FIXTURE(tempDirFixture, "realFileSystem writes atomically and leaves no temp file") {
  realFileSystem fs;
  auto target = tempDir() / "nested" / "file.txt";
  REQUIRE(fs.writeFileAtomic(target, "content"));
  CHECK(fs.readFile(target) == "content");
  CHECK_FALSE(fs.exists(target.string() + ".tmp"));
  CHECK(fs.appendFile(target, "+more"));
  CHECK(fs.readFile(target) == "content+more");
}
