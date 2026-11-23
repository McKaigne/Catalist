#ifndef CATALIST_WEB_ANILIST_HPP_
#define CATALIST_WEB_ANILIST_HPP_

#include <optional>
#include <string>
#include <vector>

#include "core/entry.hpp"
#include "core/result.hpp"
#include "web/http.hpp"

// AniList's public GraphQL API: the title search, whose matches hold both
// ids and the totals. It needs no account, and AniList also knows every
// title's MyAnimeList id. The API takes only POST requests.
namespace catalist::anilist {

inline const std::string kApiUrl = "https://graphql.anilist.co";

// One match of a search.
struct Match {
	// 0 for an id AniList does not know.
	int anilist_id = 0;
	int mal_id = 0;
	MediaType type = MediaType::kAnime;
	std::string name;
	// Empty when AniList has no English name.
	std::string english;
	// Episodes or chapters; 0 while the series is still running.
	int total_units = 0;
	int total_volumes = 0;
	// Four digits, or empty when there is no date.
	std::string year;
	// The site-wide score out of 10, 0 when there is none yet.
	double score = 0.0;
};

// The request body, and the reader for its reply.
[[nodiscard]] std::string SearchRequest(std::optional<MediaType> only, const std::string& query);
[[nodiscard]] Result<std::vector<Match>> ParseSearch(const std::string& body);

// Best first, anime before manga: up to five of each, or ten of `only`. Adult
// titles are left out. Fails when AniList cannot be reached, which the caller
// must tell apart from a search that found nothing.
[[nodiscard]] Result<std::vector<Match>> Search(const web::Fetcher& fetch,
                                                std::optional<MediaType> only,
                                                const std::string& query);

}  // namespace catalist::anilist

#endif  // CATALIST_WEB_ANILIST_HPP_
