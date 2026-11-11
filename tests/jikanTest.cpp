#include <doctest/doctest.h>

#include "jikan.hpp"
#include "support/testSupport.hpp"

namespace jikan = catalist::jikan;
using catalist::mediaType;

TEST_CASE("jikan search url encodes the query and picks the media type") {
  CHECK(jikan::searchUrl(mediaType::kAnime, "Odd Taxi") ==
        "https://api.jikan.moe/v4/anime?q=Odd%20Taxi&limit=10");
  CHECK(jikan::searchUrl(mediaType::kManga, "a&b=c") ==
        "https://api.jikan.moe/v4/manga?q=a%26b%3dc&limit=10");
  // Only unreserved characters pass through; UTF-8 is encoded byte by byte.
  CHECK(jikan::searchUrl(mediaType::kAnime, "aZ09-_.~ /é") ==
        "https://api.jikan.moe/v4/anime?q=aZ09-_.~%20%2f%c3%a9&limit=10");
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
  catalist::test::fakeWeb web;
  auto offline = jikan::search(web.fetcher(), mediaType::kAnime, "frieren");
  REQUIRE_FALSE(offline.ok());
  CHECK(offline.errorMessage() == "could not reach Jikan");
  REQUIRE(web.requested.size() == 1);
  CHECK(web.requested[0] ==
        "https://api.jikan.moe/v4/anime?q=frieren&limit=10");

  web.respond("q=frieren", R"({"data": [{"mal_id": 1, "title": "F"}]})");
  auto online = jikan::search(web.fetcher(), mediaType::kAnime, "frieren");
  REQUIRE(online.ok());
  CHECK(online.value().size() == 1);
}

TEST_CASE("jikan details count everything as out once a title has ended") {
  auto anime = jikan::parseDetails(
      R"({"data": {"episodes": 28, "status": "Finished Airing",
          "images": {"jpg": {"image_url": "https://img/a.jpg"}}}})",
      mediaType::kAnime);
  REQUIRE(anime.ok());
  CHECK(anime.value().facts.totalUnits == 28);
  CHECK(anime.value().facts.released == 28);
  CHECK_FALSE(anime.value().ongoing);
  CHECK(anime.value().imageUrl == "https://img/a.jpg");

  auto stopped = jikan::parseDetails(
      R"({"data": {"chapters": 120, "volumes": 12, "status": "Discontinued"}})",
      mediaType::kManga);
  REQUIRE(stopped.ok());
  CHECK(stopped.value().facts.released == 120);
  CHECK(stopped.value().facts.totalVolumes == 12);
}

TEST_CASE("jikan details leave the released count unknown while a title runs") {
  for (const char* status : {"Publishing", "On Hiatus", "Not yet published"}) {
    CAPTURE(status);
    auto manga = jikan::parseDetails(
        std::string(R"({"data": {"chapters": null, "status": ")") + status +
            R"("}})",
        mediaType::kManga);
    REQUIRE(manga.ok());
    CHECK(manga.value().facts.released == 0);
    CHECK(manga.value().ongoing ==
          (std::string(status) != "Not yet published"));
  }
  // An announced total is not a released one.
  auto upcoming = jikan::parseDetails(
      R"({"data": {"episodes": 12, "status": "Not yet aired"}})",
      mediaType::kAnime);
  REQUIRE(upcoming.ok());
  CHECK(upcoming.value().facts.totalUnits == 12);
  CHECK(upcoming.value().facts.released == 0);
  CHECK_FALSE(
      jikan::parseDetails(R"({"status": 404})", mediaType::kAnime).ok());
}

TEST_CASE("jikan episode pages count what aired by the given day") {
  auto today = catalist::test::requireDate("2026-10-20");
  auto page = jikan::parseEpisodes(
      R"({"pagination": {"last_visible_page": 3}, "data": [
          {"aired": "2026-10-20T00:00:00+00:00"}, {"aired": "2026-10-21"},
          {"aired": null}]})",
      today);
  REQUIRE(page.ok());
  CHECK(page.value().lastPage == 3);
  CHECK(page.value().aired == 2);
  CHECK(jikan::episodesUrl(9, 2) ==
        "https://api.jikan.moe/v4/anime/9/episodes?page=2");
  CHECK(jikan::detailsUrl(mediaType::kManga, 2) ==
        "https://api.jikan.moe/v4/manga/2");
}

TEST_CASE("jikan lookUp does not guess when an episode page is missing") {
  catalist::test::fakeWeb web;
  web.respond("anime/9", R"({"data": {"status": "Currently Airing"}})");
  web.respond("episodes?page=1",
              R"({"pagination": {"last_visible_page": 2}, "data": []})");
  auto found = jikan::lookUp(web.fetcher(), mediaType::kAnime, 9,
                             catalist::test::requireDate("2026-10-20"));
  REQUIRE(found.ok());
  CHECK(found.value().facts.released == 0);
  CHECK(web.requested.size() == 3);
}
