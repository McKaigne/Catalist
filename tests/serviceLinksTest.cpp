#include <doctest/doctest.h>

#include "catalist/model/serviceLinks.hpp"
#include "support/testSupport.hpp"

using catalist::mediaType;
using catalist::test::makeEntry;

TEST_CASE("webAddress prefers MyAnimeList, then AniList") {
  CHECK(catalist::webAddress(makeEntry().malId(52991).anilistId(154587)) ==
        "https://myanimelist.net/anime/52991");
  CHECK(catalist::webAddress(makeEntry().anilistId(30002).type(mediaType::kManga)) ==
        "https://anilist.co/manga/30002");
  CHECK(catalist::webAddress(makeEntry().title("No ids")).empty());
}

TEST_CASE("serviceBadges labels every service id in order") {
  CHECK(catalist::serviceBadges(makeEntry().malId(5).anilistId(100005)) ==
        std::vector<std::string>{"MAL #5", "AL #100005"});
  CHECK(catalist::serviceBadges(makeEntry().anilistId(7)) ==
        std::vector<std::string>{"AL #7"});
  CHECK(catalist::serviceBadges(makeEntry()).empty());
}
