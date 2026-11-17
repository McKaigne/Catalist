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
struct TitleRef {
	MediaType type = MediaType::kAnime;
	int anilist_id = 0;
	int mal_id = 0;
};

// The request bodies, and readers for their replies.
[[nodiscard]] std::string LookupRequest(const TitleRef& wanted);
[[nodiscard]] web::Answer<web::Details> ParseLookup(const std::string& body, MediaType type);
[[nodiscard]] std::string SearchRequest(MediaType type, const std::string& query);
[[nodiscard]] web::Answer<std::vector<web::Match>> ParseSearch(const std::string& body,
                                                               MediaType type);

// One request for up to kBatchSize titles of one type, by their AniList ids
// or, with `by_mal`, their MyAnimeList ids; the titles of its reply, in no
// particular order.
inline constexpr size_t kBatchSize = 50;
[[nodiscard]] std::string BatchRequest(MediaType type, const std::vector<int>& ids, bool by_mal);
[[nodiscard]] web::Answer<std::vector<web::Details>> ParseBatch(const std::string& body,
                                                                MediaType type);

// The mutation that sets a title on the account's list, and the query for
// the list entry of a title (to remove it).
[[nodiscard]] std::string SaveRequest(const Entry& title, const web::Sending& what = {});
[[nodiscard]] std::string ListEntryRequest(int media_id, const std::string& user_id);
// The query for the user's own entry of a title, and the entry of its
// reply (nothing when the title is not on the list).
[[nodiscard]] std::string EntryRequest(const Entry& title, const std::string& user_id);
[[nodiscard]] web::Answer<std::optional<web::SiteEntry>> ParseEntry(const std::string& body);

// The query for a user's whole list of one type, and the entries of its
// reply.
[[nodiscard]] std::string ListRequest(MediaType type, const std::string& user);
[[nodiscard]] web::Answer<std::vector<Entry>> ParseList(const std::string& body, MediaType type);

class Tracker : public web::Tracker {
public:
	explicit Tracker(web::Link net) : net_(std::move(net)) {}

	[[nodiscard]] Provider Site() const override { return Provider::kAniList; }
	[[nodiscard]] web::Answer<std::vector<web::Match>> Search(MediaType type,
	                                                          const std::string& query) override;
	[[nodiscard]] web::Answer<web::Details> LookUp(const Entry& known) override;
	// kBatchSize titles a request.
	[[nodiscard]] std::vector<web::Answer<web::Details>> LookUpMany(
	    const std::vector<Entry>& known) override;
	// AniList's implicit grant: its page shows a token to paste, good for a
	// year, which cannot be renewed.
	[[nodiscard]] web::Answer<web::LoginStart> StartLogin() override;
	[[nodiscard]] web::Answer<Account> FinishLogin(
	    const std::vector<std::string>& answers) override;
	[[nodiscard]] web::Answer<Account> Current(const Account& login) override;
	[[nodiscard]] web::Answer<std::vector<Entry>> Pull(MediaType type,
	                                                   const std::string& user) override;
	[[nodiscard]] web::Answer<std::optional<web::SiteEntry>> EntryOf(const Entry& title,
	                                                                 const Account& login) override;
	[[nodiscard]] web::Answer<void> Push(const Entry& title, const Account& login,
	                                     const web::Sending& what) override;
	[[nodiscard]] web::Answer<void> Remove(MediaType type, int site_id,
	                                       const Account& login) override;

protected:
	void Pause() const override { net_.pause(); }

private:
	// The reply to a query, asked as the account of `token` when there is
	// one; the failure when AniList cannot be reached or said no.
	[[nodiscard]] web::Answer<std::string> Ask(const std::string& query,
	                                           const std::string& token = "") const;

	web::Link net_;
};

}  // namespace catalist::anilist

#endif  // CATALIST_WEB_ANILIST_HPP_
