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
inline const std::string kTokenUrl = "https://kitsu.app/api/oauth/token";

// The request URLs, and readers for their replies.
[[nodiscard]] std::string SearchUrl(MediaType type, const std::string& query);
[[nodiscard]] web::Answer<std::vector<web::Match>> ParseSearch(const std::string& body,
                                                               MediaType type);
[[nodiscard]] std::string TitleUrl(MediaType type, int kitsu_id);
[[nodiscard]] web::Answer<web::Details> ParseTitle(const std::string& body, MediaType type);
// The mapping of a MyAnimeList id to Kitsu's.
[[nodiscard]] std::string MappingUrl(MediaType type, int mal_id);
// The Kitsu id the mapping names; 0 when there is none.
[[nodiscard]] int ParseMapping(const std::string& body);

// A user's library of one type: the first page's URL, and the entries of
// a page with the URL of the next ("" after the last).
[[nodiscard]] std::string LibraryUrl(MediaType type, const std::string& user_id);
[[nodiscard]] web::Answer<std::vector<Entry>> ParseLibrary(const std::string& body, MediaType type,
                                                           std::string& next);

// The user's own library entry of a title, from a reply of
// library-entries; nothing when the reply has none.
[[nodiscard]] web::Answer<std::optional<web::SiteEntry>> ParseEntry(const std::string& body);
// The attributes of a library entry, as Catalist's lists have the title.
[[nodiscard]] std::string LibraryAttributes(const Entry& title, const web::Sending& what = {});

class Tracker : public web::Tracker {
public:
	explicit Tracker(web::Link net) : net_(std::move(net)) {}

	[[nodiscard]] Provider Site() const override { return Provider::kKitsu; }
	[[nodiscard]] web::Answer<std::vector<web::Match>> Search(MediaType type,
	                                                          const std::string& query) override;
	[[nodiscard]] web::Answer<web::Details> LookUp(const Entry& known) override;
	// OAuth 2's password grant, as Kitsu has it: the email and password are
	// sent once, for a token, and not kept.
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
	// The reply to a request, or to a GET; the failure when Kitsu cannot be
	// reached or said no.
	[[nodiscard]] web::Answer<std::string> Send(const web::Request& asked) const;
	[[nodiscard]] web::Answer<std::string> Get(const std::string& url) const;

	// The token request with these fields, and the program's id when it has
	// one; the account it is for.
	[[nodiscard]] web::Answer<Account> AccountFor(
	    std::vector<std::pair<std::string, std::string>> fields);

	// The id of the account's library entry for the title; 0 when it has
	// none.
	[[nodiscard]] web::Answer<int> LibraryEntry(MediaType type, int kitsu_id,
	                                            const Account& login) const;
	// The headers of a request made as the account.
	[[nodiscard]] static std::vector<std::string> AsUser(const Account& login);

	web::Link net_;
};

}  // namespace catalist::kitsu

#endif  // CATALIST_WEB_KITSU_HPP_
