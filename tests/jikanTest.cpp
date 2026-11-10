#include <doctest/doctest.h>

#include "catalist/net/jikan.hpp"
#include "support/fakes.hpp"

namespace jikan = catalist::jikan;
using catalist::mediaType;

TEST_CASE("jikan search url encodes the query and picks the media type") {
  CHECK(jikan::searchUrl(mediaType::kAnime, "Odd Taxi") ==
        "https://api.jikan.moe/v4/anime?q=Odd%20Taxi&limit=10");
  CHECK(jikan::searchUrl(mediaType::kManga, "a&b=c") ==
        "https://api.jikan.moe/v4/manga?q=a%26b%3dc&limit=10");
}

TEST_CASE("jikan reads anime matches with their year from either field") {
  auto found = jikan::parseSearch(R"({"pagination": {}, "data": [
      {"mal_id": 52991, "title": "Sousou no Frieren", "episodes": 28,
       "score": 9.31, "year": 2023, "aired": {"from": "2023-09-29T00:00:00"}},
      {"mal_id": 21, "title": "One Piece", "episodes": null, "score": 8.7,
       "year": null, "aired": {"from": "1999-10-20T00:00:00+00:00"}},
      {"mal_id": 99, "title": "Undated", "year": null, "aired": {"from": null}}
    ]})",
                                  mediaType::kAnime);
  REQUIRE(found.ok());
  const auto& titles = found.value();
  REQUIRE(titles.size() == 3);
  CHECK(titles[0].malId == 52991);
  CHECK(titles[0].name == "Sousou no Frieren");
  CHECK(titles[0].totalUnits == 28);
  CHECK(titles[0].year == "2023");
  CHECK(titles[0].score == doctest::Approx(9.31));
  CHECK(titles[1].totalUnits == 0);
  CHECK(titles[1].year == "1999");
  CHECK(titles[2].year.empty());
  CHECK(titles[2].score == 0.0);
}

TEST_CASE("jikan reads manga chapters, volumes and publication year") {
  auto found = jikan::parseSearch(R"({"data": [
      {"mal_id": 2, "title": "Berserk", "chapters": null, "volumes": null,
       "episodes": 99, "published": {"from": "1989-08-25T00:00:00+00:00"}},
      {"mal_id": 116778, "title": "Chainsaw Man", "chapters": 97,
       "volumes": 11, "published": {"from": "2018-12-03T00:00:00+00:00"}}
    ]})",
                                  mediaType::kManga);
  REQUIRE(found.ok());
  const auto& titles = found.value();
  REQUIRE(titles.size() == 2);
  CHECK(titles[0].totalUnits == 0);
  CHECK(titles[0].year == "1989");
  CHECK(titles[1].totalUnits == 97);
  CHECK(titles[1].totalVolumes == 11);
  CHECK(titles[1].year == "2018");
}

TEST_CASE("jikan skips matches without an id or a title") {
  auto found = jikan::parseSearch(R"({"data": [
      {"title": "No id"}, {"mal_id": 5}, {"mal_id": -3, "title": "Negative"},
      {"mal_id": 6, "title": "Kept"}]})",
                                  mediaType::kAnime);
  REQUIRE(found.ok());
  REQUIRE(found.value().size() == 1);
  CHECK(found.value()[0].name == "Kept");
}

TEST_CASE("jikan rejects a reply that is not a search result") {
  CHECK_FALSE(jikan::parseSearch("<html>busy</html>", mediaType::kAnime).ok());
  CHECK_FALSE(jikan::parseSearch(R"({"status": 429})", mediaType::kAnime).ok());
  auto empty = jikan::parseSearch(R"({"data": []})", mediaType::kAnime);
  REQUIRE(empty.ok());
  CHECK(empty.value().empty());
}

TEST_CASE("jikan search tells an unreachable server from no matches") {
  catalist::test::mockHttpClient http;
  auto offline = jikan::search(http, mediaType::kAnime, "frieren");
  REQUIRE_FALSE(offline.ok());
  CHECK(offline.errorMessage() == "could not reach Jikan");
  REQUIRE(http.requested.size() == 1);
  CHECK(http.requested[0] ==
        "https://api.jikan.moe/v4/anime?q=frieren&limit=10");

  http.respond("q=frieren", R"({"data": [{"mal_id": 1, "title": "F"}]})");
  auto online = jikan::search(http, mediaType::kAnime, "frieren");
  REQUIRE(online.ok());
  CHECK(online.value().size() == 1);
}
