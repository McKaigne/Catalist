#ifndef CATALIST_WEB_MAL_HPP_
#define CATALIST_WEB_MAL_HPP_

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/entry.hpp"
#include "core/result.hpp"
#include "web/tracker.hpp"

// MyAnimeList's official API (v2): the title search and what is known about
// one title. Every request carries the program's client id, even for public
// data. It knows only its own ids, and does not say how many episodes of a
// running show have aired.
namespace catalist::mal {

inline const std::string kApiUrl = "https://api.myanimelist.net/v2";
inline const std::string kTokenUrl = "https://myanimelist.net/v1/oauth2/token";

// The request URLs, and readers for their replies.
[[nodiscard]] std::string SearchUrl(MediaType type, const std::string& query);
[[nodiscard]] web::Answer<std::vector<web::Match>> ParseSearch(const std::string& body,
                                                               MediaType type);
[[nodiscard]] std::string TitleUrl(MediaType type, int mal_id);
[[nodiscard]] web::Answer<web::Details> ParseTitle(const std::string& body, MediaType type);

// A user's public list of one type: the first page's URL, and the entries
// of a page with the URL of the next ("" after the last).
[[nodiscard]] std::string ListUrl(MediaType type, const std::string& user);
[[nodiscard]] web::Answer<std::vector<Entry>> ParseList(const std::string& body, MediaType type,
                                                        std::string& next);

// The form that sets a title on the account's list. A date the title has
// not is left out: MyAnimeList keeps the one it had.
[[nodiscard]] std::string ListStatusForm(const Entry& title, unsigned fields = site_field::kAll);

// The user's own entry of a title: its URL, and the entry of the reply
// (nothing when the title is not on the list).
[[nodiscard]] std::string EntryUrl(MediaType type, int mal_id);
[[nodiscard]] web::Answer<std::optional<web::SiteEntry>> ParseEntry(const std::string& body,
                                                                    MediaType type);

class Tracker : public web::Tracker {
public:
	explicit Tracker(web::Link net) : net_(std::move(net)) {}

	[[nodiscard]] Provider Site() const override { return Provider::kMyAnimeList; }
	[[nodiscard]] web::Answer<std::vector<web::Match>> Search(MediaType type,
	                                                          const std::string& query) override;
	[[nodiscard]] web::Answer<web::Details> LookUp(const Entry& known) override;
	// OAuth 2 with PKCE: its page sends the browser to the address the app
	// was registered with, carrying a code to paste back.
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
	// The reply to a GET with the client id; the failure when MyAnimeList
	// cannot be reached or said no.
	[[nodiscard]] web::Answer<std::string> Get(const std::string& url) const;
	// Why nothing can be asked: no client id. Empty when something can.
	[[nodiscard]] std::string CannotAsk() const;
	// The account a token reply is for; asks MyAnimeList the user's name.
	[[nodiscard]] web::Answer<Account> AccountFor(const std::optional<web::Reply>& reply);

	web::Link net_;
	// PKCE's verifier, from StartLogin to FinishLogin.
	std::string verifier_;
};

}  // namespace catalist::mal

#endif  // CATALIST_WEB_MAL_HPP_
