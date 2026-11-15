#ifndef CATALIST_ANILIST_HPP_
#define CATALIST_ANILIST_HPP_

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "entry.hpp"
#include "jikan.hpp"
#include "result.hpp"

// AniList's public GraphQL API, the other web source beside Jikan: titles
// are looked up by their MyAnimeList id, so the catalogue stays keyed on
// it. Lookups and covers ask AniList first (it needs no account and says
// how many episodes of an airing anime are out); the add search asks it
// when Jikan cannot be reached. Its API takes only POST requests.
namespace catalist::anilist {

// Posts JSON to a URL: the body of the reply, or nothing when the request
// failed. The program passes httpPost; tests pass a fake.
using poster =
    std::function<std::optional<std::string>(const std::string& url, const std::string& json)>;

inline const std::string kApiUrl = "https://graphql.anilist.co";

// The request bodies, and readers for their replies.
[[nodiscard]] std::string lookupRequest(mediaType type, int malId);
[[nodiscard]] result<jikan::details> parseLookup(const std::string& body, mediaType type);
[[nodiscard]] std::string searchRequest(mediaType type, const std::string& query);
[[nodiscard]] result<std::vector<jikan::title>> parseSearch(const std::string& body,
                                                            mediaType type);

// What AniList knows about the title with this MyAnimeList id. Fails when
// AniList cannot be reached or does not know the title.
[[nodiscard]] result<jikan::details> lookUp(const poster& post, mediaType type, int malId);

// Up to ten matches that have a MyAnimeList id, best first.
[[nodiscard]] result<std::vector<jikan::title>> search(const poster& post, mediaType type,
                                                       const std::string& query);

}  // namespace catalist::anilist

#endif  // CATALIST_ANILIST_HPP_
