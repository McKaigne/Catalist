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

// One request for up to kBatchSize titles of one type, by their AniList ids
// or, with `byMal`, their MyAnimeList ids; the titles of its reply, in no
// particular order.
inline constexpr size_t kBatchSize = 50;
[[nodiscard]] std::string batchRequest(mediaType type, const std::vector<int>& ids, bool byMal);
[[nodiscard]] result<std::vector<web::details>> parseBatch(const std::string& body, mediaType type);

// The mutation that sets a title on the account's list, and the query for
// the list entry of a title (to remove it).
[[nodiscard]] std::string saveRequest(const entry& title);
[[nodiscard]] std::string listEntryRequest(int mediaId, const std::string& userId);

class tracker : public web::iTracker {
public:
	explicit tracker(web::link net) : net_(std::move(net)) {}

	[[nodiscard]] provider site() const override { return provider::kAniList; }
	[[nodiscard]] result<std::vector<web::match>> search(mediaType type,
	                                                     const std::string& query) override;
	[[nodiscard]] result<web::details> lookUp(const entry& known) override;
	// kBatchSize titles a request.
	[[nodiscard]] std::vector<result<web::details>> lookUpMany(
	    const std::vector<entry>& known) override;
	// AniList's implicit grant: its page shows a token to paste, good for a
	// year, which cannot be renewed.
	[[nodiscard]] result<web::loginStart> startLogin() override;
	[[nodiscard]] result<account> finishLogin(const std::vector<std::string>& answers) override;
	[[nodiscard]] result<account> current(const account& login) override;
	[[nodiscard]] result<bool> push(const entry& title, const account& login) override;
	[[nodiscard]] result<bool> remove(mediaType type, int siteId, const account& login) override;

protected:
	void pause() const override { net_.pause(); }

private:
	// The reply to a query, asked as the account of `token` when there is
	// one; nothing when AniList cannot be reached or said no.
	[[nodiscard]] std::optional<std::string> ask(const std::string& query,
	                                             const std::string& token = "") const;

	web::link net_;
};

}  // namespace catalist::anilist

#endif  // CATALIST_WEB_ANILIST_HPP_
