#include <doctest/doctest.h>

#include "catalist/tui/tabs/searchTab.hpp"
#include "tui/tuiHarness.hpp"

using catalist::mediaType;
using ftxui::Event;

namespace {

const char* kAnilistBody =
    R"({"data":{"Page":{"media":[)"
    R"({"id":154587,"idMal":52991,"title":{"romaji":"Sousou no Frieren"},"episodes":28,"averageScore":91,"startDate":{"year":2023}}]}}})";

const char* kKitsuBody =
    R"({"data":[{"id":"46474","attributes":{"canonicalTitle":"Frieren","episodeCount":28,"averageRating":"90.1"}}]})";

}  // namespace

TEST_CASE("tui search types a query and adds the MAL result") {
  catalist::test::tuiHarness harness;
  harness.http->setResponse("graphql.anilist.co", {200, kAnilistBody, ""});
  catalist::searchTab tab(harness.config);
  tab.onActivate(harness.ctx);
  CHECK(tab.capturesInput());

  harness.type(tab, "frieren");
  REQUIRE(harness.press(tab, Event::Return));
  CHECK_FALSE(tab.capturesInput());
  CHECK(harness.screen(tab).find("Sousou no Frieren") != std::string::npos);

  REQUIRE(harness.press(tab, Event::Return));
  auto added = harness.db.getById(mediaType::kAnime, 52991);
  REQUIRE(added.has_value());
  CHECK(added->malId == 52991);
  CHECK(harness.screen(tab).find("in library") != std::string::npos);
}

TEST_CASE("tui search switches provider and adds kitsu results with a local id") {
  catalist::test::tuiHarness harness;
  harness.http->setResponse("kitsu.io", {200, kKitsuBody, ""});
  catalist::searchTab tab(harness.config);
  tab.onActivate(harness.ctx);
  REQUIRE(harness.press(tab, Event::Escape));
  REQUIRE(harness.press(tab, Event::Character('p')));
  REQUIRE(harness.press(tab, Event::Character('p')));

  REQUIRE(harness.press(tab, Event::Character('/')));
  harness.type(tab, "frieren");
  REQUIRE(harness.press(tab, Event::Return));
  REQUIRE(harness.press(tab, Event::Return));

  const auto& anime = harness.db.getEntries(mediaType::kAnime);
  REQUIRE(anime.size() == 1);
  CHECK(anime[0].getRemoteId("kitsu") == "46474");
  CHECK(anime[0].malId == 0);
  CHECK(anime[0].id >= 10001);
}

TEST_CASE("tui search toggles media type") {
  catalist::test::tuiHarness harness;
  catalist::searchTab tab(harness.config);
  REQUIRE(harness.press(tab, Event::Escape));
  REQUIRE(harness.press(tab, Event::Character('m')));
  CHECK(harness.screen(tab).find("Manga") != std::string::npos);
}
