#include <doctest/doctest.h>

#include "catalist/platform/httpClient.hpp"
#include "catalist/net/remoteSearch.hpp"

using catalist::mediaType;
using catalist::mockHttpClient;
using catalist::remoteSearchEngine;

TEST_CASE("remoteSearch parses AniList media results") {
  mockHttpClient http;
  http.setResponse(
      "graphql.anilist.co",
      {200,
       R"({"data":{"Page":{"media":[)"
       R"({"id":154587,"idMal":52991,"title":{"romaji":"Sousou no Frieren","english":"Frieren"},)"
       R"("episodes":28,"chapters":null,"status":"FINISHED","averageScore":91,"startDate":{"year":2023}},)"
       R"({"id":1,"idMal":null,"title":{"romaji":null,"english":null}}]}}})",
       ""});

  std::string err;
  auto results = remoteSearchEngine(http).query(mediaType::kAnime, "frieren", err, 10);
  CHECK(err == "");
  REQUIRE(results.size() == 1);
  CHECK(results[0].malId == 52991);
  CHECK(results[0].title == "Sousou no Frieren");
  CHECK(results[0].titleEnglish == "Frieren");
  CHECK(results[0].totalUnits == 28);
  CHECK(results[0].score == doctest::Approx(9.1));
  CHECK(results[0].status == "FINISHED");
  CHECK(results[0].year == "2023");
}

TEST_CASE("remoteSearch falls back to Jikan when AniList fails") {
  mockHttpClient http;
  http.setResponse(
      "api.jikan.moe",
      {200,
       R"({"pagination":{"has_next_page":false},"data":[)"
       R"({"mal_id":2,"title":"Berserk","title_english":"Berserk","chapters":null,)"
       R"("score":9.47,"status":"Publishing","year":null,"published":{"from":"1989-08-25T00:00:00+00:00"}}]})",
       ""});

  std::string err;
  auto results = remoteSearchEngine(http).query(mediaType::kManga, "berserk", err, 5);
  REQUIRE(results.size() == 1);
  CHECK(results[0].malId == 2);
  CHECK(results[0].type == mediaType::kManga);
  CHECK(results[0].score == doctest::Approx(9.47));
  CHECK(results[0].year == "1989");
}

TEST_CASE("remoteSearch reports an error when every provider fails") {
  mockHttpClient http;
  std::string err;
  auto results = remoteSearchEngine(http).query(mediaType::kAnime, "x", err);
  CHECK(results.empty());
  CHECK(err.find("Failed to contact remote providers") == 0);
}

TEST_CASE("remoteSearch urlEncode escapes reserved characters") {
  CHECK(remoteSearchEngine::urlEncode("a b&c/é") == "a%20b%26c%2f%c3%a9");
}
