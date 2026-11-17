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
[[nodiscard]] web::answer<web::details> parseLookup(const std::string& body, mediaType type);
[[nodiscard]] std::string searchRequest(mediaType type, const std::string& query);
[[nodiscard]] web::answer<std::vector<web::match>> parseSearch(const std::string& body,
                                                               mediaType type);

// One request for up to kBatchSize titles of one type, by their AniList ids
// or, with `byMal`, their MyAnimeList ids; the titles of its reply, in no
// particular order.
inline constexpr size_t kBatchSize = 50;
[[nodiscard]] std::string batchRequest(mediaType type, const std::vector<int>& ids, bool byMal);
[[nodiscard]] web::answer<std::vector<web::details>> parseBatch(const std::string& body,
                                                                mediaType type);

// The mutation that sets a title on the account's list, and the query for
// the list entry of a title (to remove it).
[[nodiscard]] std::string saveRequest(const entry& title);
[[nodiscard]] std::string listEntryRequest(int mediaId, const std::string& userId);
// The query for the user's own entry of a title, and the entry of its
// reply (nothing when the title is not on the list).
[[nodiscard]] std::string entryRequest(const entry& title, const std::string& userId);
[[nodiscard]] web::answer<std::optional<web::siteEntry>> parseEntry(const std::string& body);

// The query for a user's whole list of one type, and the entries of its
// reply.
[[nodiscard]] std::string listRequest(mediaType type, const std::string& user);
[[nodiscard]] web::answer<std::vector<entry>> parseList(const std::string& body, mediaType type);

class tracker : public web::iTracker {
public:
	explicit tracker(web::link net) : net_(std::move(net)) {}

	[[nodiscard]] provider site() const override { return provider::kAniList; }
	[[nodiscard]] web::answer<std::vector<web::match>> search(mediaType type,
	                                                          const std::string& query) override;
	[[nodiscard]] web::answer<web::details> lookUp(const entry& known) override;
	// kBatchSize titles a request.
	[[nodiscard]] std::vector<web::answer<web::details>> lookUpMany(
	    const std::vector<entry>& known) override;
	// AniList's implicit grant: its page shows a token to paste, good for a
	// year, which cannot be renewed.
	[[nodiscard]] web::answer<web::loginStart> startLogin() override;
	[[nodiscard]] web::answer<account> finishLogin(
	    const std::vector<std::string>& answers) override;
	[[nodiscard]] web::answer<account> current(const account& login) override;
	[[nodiscard]] web::answer<std::vector<entry>> pull(mediaType type,
	                                                   const std::string& user) override;
	[[nodiscard]] web::answer<std::optional<web::siteEntry>> entryOf(
	    const entry& title, const account& login) override;
	[[nodiscard]] web::answer<void> push(const entry& title, const account& login) override;
	[[nodiscard]] web::answer<void> remove(mediaType type, int siteId,
	                                       const account& login) override;

protected:
	void pause() const override { net_.pause(); }

private:
	// The reply to a query, asked as the account of `token` when there is
	// one; the failure when AniList cannot be reached or said no.
	[[nodiscard]] web::answer<std::string> ask(const std::string& query,
	                                           const std::string& token = "") const;

	web::link net_;
};

}  // namespace catalist::anilist

#endif  // CATALIST_WEB_ANILIST_HPP_
