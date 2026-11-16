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
[[nodiscard]] result<std::vector<web::match>> parseSearch(const std::string& body, mediaType type);
[[nodiscard]] std::string titleUrl(mediaType type, int malId);
[[nodiscard]] result<web::details> parseTitle(const std::string& body, mediaType type);

// The form that sets a title on the account's list. A date the title has
// not is left out: MyAnimeList keeps the one it had.
[[nodiscard]] std::string listStatusForm(const entry& title);

class tracker : public web::iTracker {
public:
	explicit tracker(web::link net) : net_(std::move(net)) {}

	[[nodiscard]] provider site() const override { return provider::kMyAnimeList; }
	[[nodiscard]] result<std::vector<web::match>> search(mediaType type,
	                                                     const std::string& query) override;
	[[nodiscard]] result<web::details> lookUp(const entry& known) override;
	// OAuth 2 with PKCE: its page sends the browser to the address the app
	// was registered with, carrying a code to paste back.
	[[nodiscard]] result<web::loginStart> startLogin() override;
	[[nodiscard]] result<account> finishLogin(const std::vector<std::string>& answers) override;
	[[nodiscard]] result<account> current(const account& login) override;
	[[nodiscard]] result<bool> push(const entry& title, const account& login) override;
	[[nodiscard]] result<bool> remove(mediaType type, int siteId, const account& login) override;

protected:
	void pause() const override { net_.pause(); }

private:
	// The reply to a GET with the client id; nothing when MyAnimeList cannot
	// be reached.
	[[nodiscard]] std::optional<std::string> get(const std::string& url) const;
	// Why nothing can be asked: no client id. Empty when something can.
	[[nodiscard]] std::string cannotAsk() const;
	// The account a token reply is for; asks MyAnimeList the user's name.
	[[nodiscard]] result<account> accountFor(const std::optional<std::string>& reply);

	web::link net_;
	// PKCE's verifier, from startLogin to finishLogin.
	std::string verifier_;
};

}  // namespace catalist::mal

#endif  // CATALIST_WEB_MAL_HPP_
