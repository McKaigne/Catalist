#include <doctest/doctest.h>

#include <memory>

#include "catalist/platform/httpClient.hpp"
#include "catalist/net/providers/providerRegistry.hpp"

TEST_CASE("kitsu provider parses search results from an injected client") {
  auto mockClient = std::make_shared<catalist::mockHttpClient>();
  mockClient->setResponse(
      "kitsu.io",
      {200,
       "{\"data\":[{\"id\":\"1234\",\"attributes\":{\"canonicalTitle\":"
       "\"Frieren: Beyond Journey\", \"episodeCount\":28,\"averageRating\":"
       "\"89.4\"}}]}",
       ""});

  catalist::providerRegistry registry(mockClient);
  auto kitsu = registry.getProvider("kitsu");
  REQUIRE(static_cast<bool>(kitsu));

  auto results = kitsu->search(catalist::mediaType::kAnime, "Frieren", 5);
  REQUIRE(results.size() == 1);
  CHECK(results[0].remoteId == "1234");
  CHECK(results[0].title == "Frieren: Beyond Journey");
  CHECK(results[0].totalUnits == 28);
  CHECK(results[0].score > 8.0);
}

namespace {

const char* kAnilistResponse =
    R"({"data":{"Page":{"media":[)"
    R"({"id":154587,"idMal":52991,"title":{"romaji":"Sousou no Frieren","english":"Frieren"},"episodes":28,"averageScore":91},)"
    R"({"id":999001,"idMal":null,"title":{"romaji":"AniList Only","english":null},"episodes":12}]}}})";

std::shared_ptr<catalist::iProvider> providerWithAnilistFixture(
    const std::string& key) {
  auto client = std::make_shared<catalist::mockHttpClient>();
  client->setResponse("graphql.anilist.co", {200, kAnilistResponse, ""});
  // Providers share ownership of the client, so they outlive the registry.
  return catalist::providerRegistry(client).getProvider(key);
}

}  // namespace

TEST_CASE("anilist provider reports AniList ids, not MAL ids") {
  auto anilist = providerWithAnilistFixture("anilist");
  REQUIRE(static_cast<bool>(anilist));
  auto results = anilist->search(catalist::mediaType::kAnime, "frieren", 5);
  REQUIRE(results.size() == 2);
  CHECK(results[0].providerKey == "anilist");
  CHECK(results[0].remoteId == "154587");
  CHECK(results[0].title == "Sousou no Frieren");
  CHECK(results[1].remoteId == "999001");
  CHECK(anilist->getWebUrl(catalist::mediaType::kAnime, results[0].remoteId) ==
        "https://anilist.co/anime/154587");
}

TEST_CASE("mal provider keeps MAL ids and skips entries without one") {
  auto mal = providerWithAnilistFixture("mal");
  REQUIRE(static_cast<bool>(mal));
  auto results = mal->search(catalist::mediaType::kAnime, "frieren", 5);
  REQUIRE(results.size() == 1);
  CHECK(results[0].remoteId == "52991");
}
