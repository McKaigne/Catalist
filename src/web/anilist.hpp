#ifndef CATALIST_WEB_ANILIST_HPP_
#define CATALIST_WEB_ANILIST_HPP_

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/entry.hpp"
#include "core/result.hpp"
#include "web/tracker.hpp"

// AniList's public GraphQL API: the title search, and what is known about
// one title (both ids, totals, how much is out). Searching and looking up
// need no account, and AniList also knows every title's MyAnimeList id. The
// API takes only POST requests.
namespace catalist::anilist {

inline const std::string kApiUrl = "https://graphql.anilist.co";

// A title to look up: by its AniList id when known, else its MyAnimeList id.
struct titleRef {
	mediaType type = mediaType::kAnime;
	int anilistId = 0;
	int malId = 0;
};

// The request bodies, and readers for their replies.
[[nodiscard]] std::string lookupRequest(const titleRef& wanted);
[[nodiscard]] result<web::details> parseLookup(const std::string& body, mediaType type);
[[nodiscard]] std::string searchRequest(mediaType type, const std::string& query);
[[nodiscard]] result<std::vector<web::match>> parseSearch(const std::string& body, mediaType type);

class tracker : public web::iTracker {
public:
	explicit tracker(web::link net) : net_(std::move(net)) {}

	[[nodiscard]] provider site() const override { return provider::kAniList; }
	[[nodiscard]] result<std::vector<web::match>> search(mediaType type,
	                                                     const std::string& query) override;
	[[nodiscard]] result<web::details> lookUp(const entry& known) override;

private:
	// The reply to a query; nothing when AniList cannot be reached.
	[[nodiscard]] std::optional<std::string> ask(const std::string& query) const;

	web::link net_;
};

}  // namespace catalist::anilist

#endif  // CATALIST_WEB_ANILIST_HPP_
