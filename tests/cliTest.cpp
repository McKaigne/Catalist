#include <doctest/doctest.h>

#include <iostream>

#include "catalist/cli.hpp"
#include "catalist/database.hpp"
#include "support/testSupport.hpp"

using catalist::CliDispatcher;
using catalist::MediaType;
using catalist::SortKey;
using catalist::test::makeEntry;
using catalist::test::streamCapture;
using catalist::test::tempDirFixture;

TEST_CASE("parseRatingSpec handles exact relational range and unrated") {
  int minScore = -1;
  int maxScore = -1;
  bool unrated = false;

  CHECK(CliDispatcher::parseRatingSpec("9", minScore, maxScore, unrated));
  CHECK(minScore == 9);
  CHECK(maxScore == 9);

  CHECK(CliDispatcher::parseRatingSpec(">=8", minScore, maxScore, unrated));
  CHECK(minScore == 8);
  CHECK(maxScore == 10);

  CHECK(CliDispatcher::parseRatingSpec("<=6", minScore, maxScore, unrated));
  CHECK(minScore == 0);
  CHECK(maxScore == 6);

  CHECK(CliDispatcher::parseRatingSpec("7..9", minScore, maxScore, unrated));
  CHECK(minScore == 7);
  CHECK(maxScore == 9);

  CHECK(CliDispatcher::parseRatingSpec("unrated", minScore, maxScore, unrated));
  CHECK(unrated);
  CHECK(minScore == 0);
}

TEST_CASE("formatJsonEntry includes id and title") {
  catalist::Entry e = makeEntry().id(42).title("Odd Taxi").score(8);
  auto json = CliDispatcher::formatJsonEntry(e);
  CHECK(json.find("\"id\":42") != std::string::npos);
  CHECK(json.find("\"title\":\"Odd Taxi\"") != std::string::npos);
}

TEST_CASE_FIXTURE(tempDirFixture, "cli add then step progress then rm") {
  catalist::Database db(tempDir());
  streamCapture out(std::cout);

  std::vector<std::string> addArgs = {"Dungeon Meshi", "-t", "anime", "-s",
                                      "watching", "-e", "24", "-p", "10",
                                      "-r", "9"};
  REQUIRE(CliDispatcher::executeAdd(addArgs, db) == 0);

  auto entries = db.filterEntries(MediaType::Anime, std::nullopt, -1,
                                  "Dungeon Meshi", SortKey::Title, false);
  REQUIRE(entries.size() == 1);
  int id = entries[0].id;

  REQUIRE(CliDispatcher::executeSet({std::to_string(id), "-p", "a", "2"}, db) ==
          0);
  auto stepped = db.getById(MediaType::Anime, id);
  REQUIRE(stepped.has_value());
  CHECK(stepped->progress == 12);

  REQUIRE(CliDispatcher::executeRm({std::to_string(id), "-y"}, db) == 0);
  CHECK_FALSE(db.getById(MediaType::Anime, id).has_value());
}

TEST_CASE("cli config list and completion generators succeed") {
  catalist::AppConfig config;
  streamCapture out(std::cout);
  CHECK(CliDispatcher::executeConfig({"list"}, config) == 0);
  CHECK(CliDispatcher::executeCompletion({"bash"}) == 0);
  CHECK(CliDispatcher::executeCompletion({"zsh"}) == 0);
  CHECK(CliDispatcher::executeCompletion({"fish"}) == 0);
}
