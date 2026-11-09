#include <doctest/doctest.h>

#include <memory>

#include "catalist/platform/httpClient.hpp"
#include "catalist/provider.hpp"

TEST_CASE("kitsu provider parses search results from an injected client") {
  auto mockClient = std::make_shared<catalist::mockHttpClient>();
  mockClient->setResponse(
      "kitsu.io",
      {200,
       "{\"data\":[{\"id\":\"1234\",\"attributes\":{\"canonicalTitle\":"
       "\"Frieren: Beyond Journey\", \"episodeCount\":28,\"averageRating\":"
       "\"89.4\"}}]}",
       "", 0, ""});

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
