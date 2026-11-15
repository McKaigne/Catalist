#ifndef CATALIST_ANILIST_HPP_
#define CATALIST_ANILIST_HPP_

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "entry.hpp"
#include "result.hpp"

// AniList's public GraphQL API, the program's one web source: the title
// search, and what is known about one title (both ids, totals, how much is
// out, the cover). It needs no account and also knows every title's
// MyAnimeList id, so MyAnimeList users are served from it too. The API takes
// only POST requests.
namespace catalist::anilist {

// Posts JSON to a URL: the body of the reply, or nothing when the request
// failed. The program passes httpPost; tests pass a fake.
using poster =
    std::function<std::optional<std::string>(const std::string& url, const std::string& json)>;

inline const std::string kApiUrl = "https://graphql.anilist.co";

// A title to look up: by its AniList id when known, else its MyAnimeList id.
struct titleRef {
	mediaType type = mediaType::kAnime;
	int anilistId = 0;
	int malId = 0;
};
[[nodiscard]] titleRef refOf(const entry& e);

// One search match.
struct title {
	int anilistId = 0;
	int malId = 0;  // 0 when MyAnimeList does not list it
	std::string name;
	// Episodes or chapters; 0 while the series is still running.
	int totalUnits = 0;
	int totalVolumes = 0;
	// Four digits, or empty when there is no date.
	std::string year;
	// The site-wide score out of 10, 0 when there is none yet.
	double score = 0.0;
};

// What AniList knows about one title.
struct details {
	std::string name;
	// The ids, totals and, once the title has ended, everything as released;
	// for an anime still airing, the episodes aired so far.
	webFacts facts;
	// Still airing or publishing.
	bool ongoing = false;
	// Empty when there is no cover.
	std::string imageUrl;
};

// The request bodies, and readers for their replies.
[[nodiscard]] std::string lookupRequest(const titleRef& wanted);
[[nodiscard]] result<details> parseLookup(const std::string& body, mediaType type);
[[nodiscard]] std::string searchRequest(mediaType type, const std::string& query);
[[nodiscard]] result<std::vector<title>> parseSearch(const std::string& body, mediaType type);

// Fails when AniList cannot be reached or does not know the title.
[[nodiscard]] result<details> lookUp(const poster& post, const titleRef& wanted);

// Up to ten matches, best first. Fails when AniList cannot be reached, which
// the caller must tell apart from a search that simply found nothing.
[[nodiscard]] result<std::vector<title>> search(const poster& post, mediaType type,
                                                const std::string& query);

}  // namespace catalist::anilist

#endif  // CATALIST_ANILIST_HPP_
