#ifndef CATALIST_WEB_KITSU_HPP_
#define CATALIST_WEB_KITSU_HPP_

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/entry.hpp"
#include "core/result.hpp"
#include "web/tracker.hpp"

// Kitsu's public JSON:API: the title search and what is known about one
// title. Its "mappings" give each title's MyAnimeList and AniList ids, and
// find a title by its MyAnimeList id. It does not say how many episodes of
// a running show have aired.
namespace catalist::kitsu {

inline const std::string kApiUrl = "https://kitsu.app/api/edge";

// The request URLs, and readers for their replies.
[[nodiscard]] std::string searchUrl(mediaType type, const std::string& query);
[[nodiscard]] result<std::vector<web::match>> parseSearch(const std::string& body, mediaType type);
[[nodiscard]] std::string titleUrl(mediaType type, int kitsuId);
[[nodiscard]] result<web::details> parseTitle(const std::string& body, mediaType type);
// The mapping of a MyAnimeList id to Kitsu's.
[[nodiscard]] std::string mappingUrl(mediaType type, int malId);
// The Kitsu id the mapping names; 0 when there is none.
[[nodiscard]] int parseMapping(const std::string& body);

class tracker : public web::iTracker {
public:
	explicit tracker(web::link net) : net_(std::move(net)) {}

	[[nodiscard]] provider site() const override { return provider::kKitsu; }
	[[nodiscard]] result<std::vector<web::match>> search(mediaType type,
	                                                     const std::string& query) override;
	[[nodiscard]] result<web::details> lookUp(const entry& known) override;

private:
	// The reply to a GET; nothing when Kitsu cannot be reached.
	[[nodiscard]] std::optional<std::string> get(const std::string& url) const;

	web::link net_;
};

}  // namespace catalist::kitsu

#endif  // CATALIST_WEB_KITSU_HPP_
