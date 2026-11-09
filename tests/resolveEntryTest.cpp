#include <doctest/doctest.h>

#include <iostream>

#include "catalist/cli.hpp"
#include "catalist/cli/resolveEntry.hpp"
#include "support/sampleLibrary.hpp"
#include "support/testSupport.hpp"

using catalist::mediaType;
using catalist::resolveEntry;
using catalist::resolveFailure;
using catalist::test::makeEntry;
using catalist::test::tempDirFixture;

namespace {

struct libraryFixture : tempDirFixture {
  libraryFixture() : db(tempDir()) { catalist::test::seedSampleLibrary(db); }
  catalist::database db;
};

}  // namespace

TEST_CASE_FIXTURE(libraryFixture, "resolveEntry finds anime and manga by id") {
  auto anime = resolveEntry(db, "44265");
  REQUIRE(anime.ok());
  CHECK(anime.value().title == "Odd Taxi");

  auto manga = resolveEntry(db, "116778");
  REQUIRE(manga.ok());
  CHECK(manga.value().type == mediaType::kManga);
}

TEST_CASE_FIXTURE(libraryFixture, "resolveEntry finds by mal id") {
  db.addEntry(makeEntry().id(20001).malId(777).title("Local copy"));
  auto byMal = resolveEntry(db, "777");
  REQUIRE(byMal.ok());
  CHECK(byMal.value().id == 20001);
}

TEST_CASE_FIXTURE(libraryFixture, "resolveEntry matches exact title before substrings") {
  db.addEntry(makeEntry().id(20002).title("Odd Taxi: In the Woods"));
  auto exact = resolveEntry(db, "ODD TAXI");
  REQUIRE(exact.ok());
  CHECK(exact.value().id == 44265);
}

TEST_CASE_FIXTURE(libraryFixture, "resolveEntry exact manga title beats anime substrings") {
  db.addEntry(makeEntry().id(20003).title("Berserk Recap Movie"));
  auto exact = resolveEntry(db, "berserk");
  REQUIRE(exact.ok());
  CHECK(exact.value().type == mediaType::kManga);
}

TEST_CASE_FIXTURE(libraryFixture, "resolveEntry falls back to title when id is unknown") {
  db.addEntry(makeEntry().id(20004).title("86"));
  auto numericTitle = resolveEntry(db, "86");
  REQUIRE(numericTitle.ok());
  CHECK(numericTitle.value().id == 20004);
}

TEST_CASE_FIXTURE(libraryFixture, "resolveEntry reports ambiguity with candidates") {
  auto ambiguous = resolveEntry(db, "o");
  REQUIRE_FALSE(ambiguous.ok());
  CHECK(ambiguous.errorValue().kind == resolveFailure::kAmbiguous);
  CHECK(ambiguous.errorValue().query == "o");
  CHECK(ambiguous.errorValue().candidates.size() == 4);
}

TEST_CASE_FIXTURE(libraryFixture, "resolveEntry reports not found") {
  auto missing = resolveEntry(db, "nothing like this");
  REQUIRE_FALSE(missing.ok());
  CHECK(missing.errorValue().kind == resolveFailure::kNotFound);
  CHECK(missing.errorValue().candidates.empty());
}

TEST_CASE_FIXTURE(libraryFixture, "resolveEntry treats an oversized number as a title") {
  auto huge = resolveEntry(db, "99999999999999999999");
  REQUIRE_FALSE(huge.ok());
  CHECK(huge.errorValue().kind == resolveFailure::kNotFound);
}

TEST_CASE_FIXTURE(libraryFixture, "cli show lists candidates for an ambiguous query") {
  catalist::test::streamCapture out(std::cout);
  catalist::test::streamCapture err(std::cerr);
  CHECK(catalist::cliDispatcher::executeShow({"o"}, db) == 1);
  CHECK(out.text() == "");
  CHECK(err.text() ==
        "error: ambiguous query 'o'. Matches multiple entries:\n"
        "  - [52991] Sousou no Frieren (anime)\n"
        "  - [44265] Odd Taxi (anime)\n"
        "  - [10001] A Very Long Title That Will Definitely Be Truncated "
        "(anime)\n"
        "  - [10002] Ghost, \"in\" the Shell (anime)\n\n");
}
