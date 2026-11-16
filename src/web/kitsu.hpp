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
[[nodiscard]] std::string searchUrl(mediaType type, const std::string& query);
[[nodiscard]] result<std::vector<web::match>> parseSearch(const std::string& body, mediaType type);
[[nodiscard]] std::string titleUrl(mediaType type, int kitsuId);
[[nodiscard]] result<web::details> parseTitle(const std::string& body, mediaType type);
// The mapping of a MyAnimeList id to Kitsu's.
[[nodiscard]] std::string mappingUrl(mediaType type, int malId);
// The Kitsu id the mapping names; 0 when there is none.
[[nodiscard]] int parseMapping(const std::string& body);

// The attributes of a library entry, as Catalist's lists have the title.
[[nodiscard]] std::string libraryAttributes(const entry& title);

class tracker : public web::iTracker {
public:
	explicit tracker(web::link net) : net_(std::move(net)) {}

	[[nodiscard]] provider site() const override { return provider::kKitsu; }
	[[nodiscard]] result<std::vector<web::match>> search(mediaType type,
	                                                     const std::string& query) override;
	[[nodiscard]] result<web::details> lookUp(const entry& known) override;
	// OAuth 2's password grant, as Kitsu has it: the email and password are
	// sent once, for a token, and not kept.
	[[nodiscard]] result<web::loginStart> startLogin() override;
	[[nodiscard]] result<account> finishLogin(const std::vector<std::string>& answers) override;
	[[nodiscard]] result<account> current(const account& login) override;
	[[nodiscard]] result<bool> push(const entry& title, const account& login) override;
	[[nodiscard]] result<bool> remove(mediaType type, int siteId, const account& login) override;

protected:
	void pause() const override { net_.pause(); }

private:
	// The reply to a GET; nothing when Kitsu cannot be reached.
	[[nodiscard]] std::optional<std::string> get(const std::string& url) const;

	// The token request with these fields, and the program's id when it has
	// one; the account it is for.
	[[nodiscard]] result<account> accountFor(
	    std::vector<std::pair<std::string, std::string>> fields);

	// The id of the account's library entry for the title; 0 when it has
	// none. Nothing when Kitsu cannot be reached.
	[[nodiscard]] std::optional<int> libraryEntry(mediaType type, int kitsuId,
	                                              const account& login) const;
	// The headers of a request made as the account.
	[[nodiscard]] static std::vector<std::string> asUser(const account& login);

	web::link net_;
};

}  // namespace catalist::kitsu

#endif  // CATALIST_WEB_KITSU_HPP_
