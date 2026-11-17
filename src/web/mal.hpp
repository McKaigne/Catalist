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
[[nodiscard]] std::string searchUrl(mediaType type, const std::string& query);
[[nodiscard]] web::answer<std::vector<web::match>> parseSearch(const std::string& body,
                                                               mediaType type);
[[nodiscard]] std::string titleUrl(mediaType type, int malId);
[[nodiscard]] web::answer<web::details> parseTitle(const std::string& body, mediaType type);

// A user's public list of one type: the first page's URL, and the entries
// of a page with the URL of the next ("" after the last).
[[nodiscard]] std::string listUrl(mediaType type, const std::string& user);
[[nodiscard]] web::answer<std::vector<entry>> parseList(const std::string& body, mediaType type,
                                                        std::string& next);

// The form that sets a title on the account's list. A date the title has
// not is left out: MyAnimeList keeps the one it had.
[[nodiscard]] std::string listStatusForm(const entry& title);

class tracker : public web::iTracker {
public:
	explicit tracker(web::link net) : net_(std::move(net)) {}

	[[nodiscard]] provider site() const override { return provider::kMyAnimeList; }
	[[nodiscard]] web::answer<std::vector<web::match>> search(mediaType type,
	                                                          const std::string& query) override;
	[[nodiscard]] web::answer<web::details> lookUp(const entry& known) override;
	// OAuth 2 with PKCE: its page sends the browser to the address the app
	// was registered with, carrying a code to paste back.
	[[nodiscard]] web::answer<web::loginStart> startLogin() override;
	[[nodiscard]] web::answer<account> finishLogin(
	    const std::vector<std::string>& answers) override;
	[[nodiscard]] web::answer<account> current(const account& login) override;
	[[nodiscard]] web::answer<std::vector<entry>> pull(mediaType type,
	                                                   const std::string& user) override;
	[[nodiscard]] web::answer<void> push(const entry& title, const account& login) override;
	[[nodiscard]] web::answer<void> remove(mediaType type, int siteId,
	                                       const account& login) override;

protected:
	void pause() const override { net_.pause(); }

private:
	// The reply to a GET with the client id; the failure when MyAnimeList
	// cannot be reached or said no.
	[[nodiscard]] web::answer<std::string> get(const std::string& url) const;
	// Why nothing can be asked: no client id. Empty when something can.
	[[nodiscard]] std::string cannotAsk() const;
	// The account a token reply is for; asks MyAnimeList the user's name.
	[[nodiscard]] web::answer<account> accountFor(const std::optional<web::reply>& reply);

	web::link net_;
	// PKCE's verifier, from startLogin to finishLogin.
	std::string verifier_;
};

}  // namespace catalist::mal

#endif  // CATALIST_WEB_MAL_HPP_
