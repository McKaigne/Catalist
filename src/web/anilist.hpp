#ifndef CATALIST_WEB_ANILIST_HPP_
#define CATALIST_WEB_ANILIST_HPP_

#include <string>
#include <vector>

#include "core/entry.hpp"
#include "core/result.hpp"
#include "web/http.hpp"

// AniList's public GraphQL API: the title search, and what is known about
// one title (both ids, totals, how much is out). Neither needs an account,
// and AniList also knows every title's MyAnimeList id. The API takes only
// POST requests.
namespace catalist::anilist {

inline const std::string kApiUrl = "https://graphql.anilist.co";

// A title to look up: by its AniList id when known, else its MyAnimeList id.
struct TitleRef {
	MediaType type = MediaType::kAnime;
	int anilist_id = 0;
	int mal_id = 0;
};

// One match of a search.
struct Match {
	// 0 for an id AniList does not know.
	int anilist_id = 0;
	int mal_id = 0;
	std::string name;
	// Episodes or chapters; 0 while the series is still running.
	int total_units = 0;
	int total_volumes = 0;
	// Four digits, or empty when there is no date.
	std::string year;
	// The site-wide score out of 10, 0 when there is none yet.
	double score = 0.0;
};

// What AniList knows about one title.
struct Details {
	std::string name;
	// The ids, totals and, once the title has ended, everything as released;
	// for an anime still airing, the episodes aired so far.
	WebFacts facts;
	// Still airing or publishing.
	bool ongoing = false;
};

// The request bodies, and readers for their replies.
[[nodiscard]] std::string LookupRequest(const TitleRef& wanted);
[[nodiscard]] Result<Details> ParseLookup(const std::string& body, MediaType type);
[[nodiscard]] std::string SearchRequest(MediaType type, const std::string& query);
[[nodiscard]] Result<std::vector<Match>> ParseSearch(const std::string& body, MediaType type);

// Up to ten matches, best first. Fails when AniList cannot be reached, which
// the caller must tell apart from a search that found nothing.
[[nodiscard]] Result<std::vector<Match>> Search(const web::Fetcher& fetch, MediaType type,
                                                const std::string& query);

// The title by its AniList id, else its MyAnimeList id. Fails when it has
// neither, when AniList does not know it, or cannot be reached.
[[nodiscard]] Result<Details> LookUp(const web::Fetcher& fetch, const Entry& known);

}  // namespace catalist::anilist

#endif  // CATALIST_WEB_ANILIST_HPP_
