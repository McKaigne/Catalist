#ifndef CATALIST_WEB_ANILIST_HPP_
#define CATALIST_WEB_ANILIST_HPP_

#include <optional>
#include <string>
#include <vector>

#include "core/entry.hpp"
#include "core/result.hpp"
#include "web/http.hpp"

namespace catalist::anilist {

inline const std::string kApiUrl = "https://graphql.anilist.co";

struct Match {
	int anilist_id = 0;
	int mal_id = 0;
	MediaType type = MediaType::kAnime;
	std::string name;
	std::string english;
	int total_units = 0;
	int total_volumes = 0;
	std::string year;
	double score = 0.0;
};

[[nodiscard]] std::string SearchRequest(std::optional<MediaType> only, const std::string& query);
[[nodiscard]] Result<std::vector<Match>> ParseSearch(const std::string& body);

[[nodiscard]] Result<std::vector<Match>> Search(const web::Fetcher& fetch,
                                                std::optional<MediaType> only,
                                                const std::string& query);

}  // namespace catalist::anilist

#endif  // CATALIST_WEB_ANILIST_HPP_
