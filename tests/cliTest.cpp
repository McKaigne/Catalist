#include <doctest/doctest.h>

#include <iostream>

#include "catalist/cli/args.hpp"
#include "catalist/cli/commands/commands.hpp"
#include "catalist/cli/output.hpp"
#include "catalist/storage/database.hpp"
#include "support/testSupport.hpp"

using catalist::cli::formatJsonEntry;
using catalist::cli::parseRatingSpec;
using catalist::mediaType;
using catalist::sortKey;
using catalist::test::makeEntry;
using catalist::test::streamCapture;
using catalist::test::tempDirFixture;

TEST_CASE("parseRatingSpec handles exact relational range and unrated") {
  auto exact = parseRatingSpec("9");
  REQUIRE(exact.has_value());
  CHECK(exact->minScore == 9);
  CHECK(exact->maxScore == 9);

  auto atLeast = parseRatingSpec(">=8");
  REQUIRE(atLeast.has_value());
  CHECK(atLeast->minScore == 8);
  CHECK(atLeast->maxScore == 10);

  auto atMost = parseRatingSpec("<=6");
  REQUIRE(atMost.has_value());
  CHECK(atMost->minScore == 0);
  CHECK(atMost->maxScore == 6);

  auto range = parseRatingSpec("7..9");
  REQUIRE(range.has_value());
  CHECK(range->minScore == 7);
  CHECK(range->maxScore == 9);

  auto unrated = parseRatingSpec("unrated");
  REQUIRE(unrated.has_value());
  CHECK(unrated->unratedOnly);
  CHECK(unrated->minScore == 0);
}

TEST_CASE("formatJsonEntry includes id and title") {
  catalist::entry e = makeEntry().id(42).title("Odd Taxi").score(8);
  auto json = formatJsonEntry(e);
  CHECK(json.find("\"id\":42") != std::string::npos);
  CHECK(json.find("\"title\":\"Odd Taxi\"") != std::string::npos);
}

TEST_CASE_FIXTURE(tempDirFixture, "cli add then step progress then rm") {
  catalist::database db(fileSystem(), clock(), tempDir());
  catalist::test::cliHarness harness(db, fileSystem(), clock(), tempDir());
  streamCapture out(std::cout);

  std::vector<std::string> addArgs = {"Dungeon Meshi", "-t", "anime", "-s",
                                      "watching", "-e", "24", "-p", "10",
                                      "-r", "9"};
  REQUIRE(catalist::cli::runAdd(addArgs, harness.context) == 0);

  auto entries = db.filterEntries(mediaType::kAnime, std::nullopt, std::nullopt,
                                  "Dungeon Meshi", sortKey::kTitle, false);
  REQUIRE(entries.size() == 1);
  int id = entries[0].id;

  REQUIRE(catalist::cli::runSet({std::to_string(id), "-p", "a", "2"},
                                 harness.context) == 0);
  auto stepped = db.getById(mediaType::kAnime, id);
  REQUIRE(stepped.has_value());
  CHECK(stepped->progress == 12);
  CHECK(catalist::formatDate(stepped->startDate) == "2026-10-20");
  CHECK(catalist::formatDate(stepped->updatedAt) == "2026-10-20");

  REQUIRE(catalist::cli::runRm({std::to_string(id), "-y"}, harness.context) == 0);
  CHECK_FALSE(db.getById(mediaType::kAnime, id).has_value());
}

TEST_CASE_FIXTURE(tempDirFixture, "cli config list and completion generators succeed") {
  catalist::database db(fileSystem(), clock(), tempDir());
  catalist::test::cliHarness harness(db, fileSystem(), clock(), tempDir());
  streamCapture out(std::cout);
  CHECK(catalist::cli::runConfig({"list"}, harness.context) == 0);
  CHECK(catalist::cli::runCompletion({"bash"}, harness.context) == 0);
  CHECK(catalist::cli::runCompletion({"zsh"}, harness.context) == 0);
  CHECK(catalist::cli::runCompletion({"fish"}, harness.context) == 0);
}

TEST_CASE("formatJsonEntry escapes quote backslash newline and control characters") {
  catalist::entry e = makeEntry()
                          .id(1)
                          .title("say \"hi\" \\ back")
                          .notes("line1\nline2\r\ttab\b\f\x01\x1f end");
  auto json = formatJsonEntry(e);
  CHECK(json.find("\"title\":\"say \\\"hi\\\" \\\\ back\"") != std::string::npos);
  CHECK(json.find("\"notes\":\"line1\\nline2\\r\\ttab\\b\\f\\u0001\\u001f end\"") !=
        std::string::npos);
}

TEST_CASE("formatJsonEntry passes utf8 bytes through unescaped") {
  catalist::entry e = makeEntry().id(1).title("葬送のフリーレン");
  auto json = formatJsonEntry(e);
  CHECK(json.find("\"title\":\"葬送のフリーレン\"") != std::string::npos);
}

namespace {

struct ratingSpec {
  bool accepted = false;
  int minScore = 0;
  int maxScore = 0;
  bool unrated = false;
};

ratingSpec parseRating(const std::string& spec) {
  auto parsed = parseRatingSpec(spec);
  if (!parsed)
    return {};
  return {true, parsed->minScore, parsed->maxScore, parsed->unratedOnly};
}

}  // namespace

TEST_CASE("parseRatingSpec strict greater and less than") {
  auto greater = parseRating(">8");
  CHECK(greater.accepted);
  CHECK(greater.minScore == 9);
  CHECK(greater.maxScore == 10);

  auto less = parseRating("<5");
  CHECK(less.accepted);
  CHECK(less.minScore == 0);
  CHECK(less.maxScore == 4);

  CHECK(parseRating(">9").accepted);
  CHECK_FALSE(parseRating(">10").accepted);
  CHECK(parseRating("<1").accepted);
  CHECK_FALSE(parseRating("<0").accepted);
  CHECK_FALSE(parseRating("<=11").accepted);
}

TEST_CASE("parseRatingSpec trims quotes and whitespace") {
  auto quoted = parseRating("\"9\"");
  CHECK(quoted.accepted);
  CHECK(quoted.minScore == 9);
  CHECK(quoted.maxScore == 9);

  auto padded = parseRating("  >=7 ");
  CHECK(padded.accepted);
  CHECK(padded.minScore == 7);
  CHECK(padded.maxScore == 10);
}

TEST_CASE("parseRatingSpec treats zero as unrated") {
  auto zero = parseRating("0");
  CHECK(zero.accepted);
  CHECK(zero.unrated);
  CHECK(zero.maxScore == 0);
}

TEST_CASE("parseRatingSpec rejects invalid input") {
  CHECK_FALSE(parseRating("abc").accepted);
  CHECK_FALSE(parseRating("").accepted);
  CHECK_FALSE(parseRating("11").accepted);
  CHECK_FALSE(parseRating("-1").accepted);
  CHECK_FALSE(parseRating("9..7").accepted);
  CHECK_FALSE(parseRating("7..").accepted);
  CHECK_FALSE(parseRating(">=x").accepted);
}

TEST_CASE_FIXTURE(tempDirFixture, "cli add rejects a malformed date and set clears one") {
  catalist::database db(fileSystem(), clock(), tempDir());
  catalist::test::cliHarness harness(db, fileSystem(), clock(), tempDir());
  streamCapture out(std::cout);
  streamCapture err(std::cerr);

  CHECK(catalist::cli::runAdd({"X", "--start", "soon"}, harness.context) == 2);
  CHECK(err.text() ==
        "error: invalid date 'soon' for '--start'. Expected YYYY-MM-DD.\n");
  CHECK(db.getEntries(mediaType::kAnime).empty());

  REQUIRE(catalist::cli::runAdd({"X", "--finish", "2026-01-02"},
                                 harness.context) == 0);
  REQUIRE(catalist::cli::runSet({"X", "--finish", ""}, harness.context) == 0);
  auto updated = db.getEntries(mediaType::kAnime).at(0);
  CHECK_FALSE(updated.finishDate.has_value());
  CHECK(catalist::formatDate(updated.startDate) == "2026-10-20");
}
