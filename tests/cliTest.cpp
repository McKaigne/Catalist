#include <doctest/doctest.h>

#include <iostream>

#include "catalist/cli.hpp"
#include "catalist/database.hpp"
#include "support/testSupport.hpp"

using catalist::cliDispatcher;
using catalist::mediaType;
using catalist::sortKey;
using catalist::test::makeEntry;
using catalist::test::streamCapture;
using catalist::test::tempDirFixture;

TEST_CASE("parseRatingSpec handles exact relational range and unrated") {
  int minScore = -1;
  int maxScore = -1;
  bool unrated = false;

  CHECK(cliDispatcher::parseRatingSpec("9", minScore, maxScore, unrated));
  CHECK(minScore == 9);
  CHECK(maxScore == 9);

  CHECK(cliDispatcher::parseRatingSpec(">=8", minScore, maxScore, unrated));
  CHECK(minScore == 8);
  CHECK(maxScore == 10);

  CHECK(cliDispatcher::parseRatingSpec("<=6", minScore, maxScore, unrated));
  CHECK(minScore == 0);
  CHECK(maxScore == 6);

  CHECK(cliDispatcher::parseRatingSpec("7..9", minScore, maxScore, unrated));
  CHECK(minScore == 7);
  CHECK(maxScore == 9);

  CHECK(cliDispatcher::parseRatingSpec("unrated", minScore, maxScore, unrated));
  CHECK(unrated);
  CHECK(minScore == 0);
}

TEST_CASE("formatJsonEntry includes id and title") {
  catalist::entry e = makeEntry().id(42).title("Odd Taxi").score(8);
  auto json = cliDispatcher::formatJsonEntry(e);
  CHECK(json.find("\"id\":42") != std::string::npos);
  CHECK(json.find("\"title\":\"Odd Taxi\"") != std::string::npos);
}

TEST_CASE_FIXTURE(tempDirFixture, "cli add then step progress then rm") {
  catalist::database db(tempDir());
  streamCapture out(std::cout);

  std::vector<std::string> addArgs = {"Dungeon Meshi", "-t", "anime", "-s",
                                      "watching", "-e", "24", "-p", "10",
                                      "-r", "9"};
  REQUIRE(cliDispatcher::executeAdd(addArgs, db) == 0);

  auto entries = db.filterEntries(mediaType::Anime, std::nullopt, -1,
                                  "Dungeon Meshi", sortKey::Title, false);
  REQUIRE(entries.size() == 1);
  int id = entries[0].id;

  REQUIRE(cliDispatcher::executeSet({std::to_string(id), "-p", "a", "2"}, db) ==
          0);
  auto stepped = db.getById(mediaType::Anime, id);
  REQUIRE(stepped.has_value());
  CHECK(stepped->progress == 12);

  REQUIRE(cliDispatcher::executeRm({std::to_string(id), "-y"}, db) == 0);
  CHECK_FALSE(db.getById(mediaType::Anime, id).has_value());
}

TEST_CASE("cli config list and completion generators succeed") {
  catalist::appConfig config;
  streamCapture out(std::cout);
  CHECK(cliDispatcher::executeConfig({"list"}, config) == 0);
  CHECK(cliDispatcher::executeCompletion({"bash"}) == 0);
  CHECK(cliDispatcher::executeCompletion({"zsh"}) == 0);
  CHECK(cliDispatcher::executeCompletion({"fish"}) == 0);
}

TEST_CASE("formatJsonEntry escapes quote backslash newline and control characters") {
  catalist::entry e = makeEntry()
                          .id(1)
                          .title("say \"hi\" \\ back")
                          .notes("line1\nline2\r\ttab\b\f\x01\x1f end");
  auto json = cliDispatcher::formatJsonEntry(e);
  CHECK(json.find("\"title\":\"say \\\"hi\\\" \\\\ back\"") != std::string::npos);
  CHECK(json.find("\"notes\":\"line1\\nline2\\r\\ttab\\b\\f\\u0001\\u001f end\"") !=
        std::string::npos);
}

TEST_CASE("formatJsonEntry passes utf8 bytes through unescaped") {
  catalist::entry e = makeEntry().id(1).title("葬送のフリーレン");
  auto json = cliDispatcher::formatJsonEntry(e);
  CHECK(json.find("\"title\":\"葬送のフリーレン\"") != std::string::npos);
}

namespace {

struct ratingSpec {
  bool accepted = false;
  int minScore = -1;
  int maxScore = -1;
  bool unrated = false;
};

ratingSpec parseRating(const std::string& spec) {
  ratingSpec parsed;
  parsed.accepted = cliDispatcher::parseRatingSpec(
      spec, parsed.minScore, parsed.maxScore, parsed.unrated);
  return parsed;
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
