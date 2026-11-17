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
[[nodiscard]] web::answer<std::vector<web::match>> parseSearch(const std::string& body,
                                                               mediaType type);
[[nodiscard]] std::string titleUrl(mediaType type, int kitsuId);
[[nodiscard]] web::answer<web::details> parseTitle(const std::string& body, mediaType type);
// The mapping of a MyAnimeList id to Kitsu's.
[[nodiscard]] std::string mappingUrl(mediaType type, int malId);
// The Kitsu id the mapping names; 0 when there is none.
[[nodiscard]] int parseMapping(const std::string& body);

// A user's library of one type: the first page's URL, and the entries of
// a page with the URL of the next ("" after the last).
[[nodiscard]] std::string libraryUrl(mediaType type, const std::string& userId);
[[nodiscard]] web::answer<std::vector<entry>> parseLibrary(const std::string& body, mediaType type,
                                                           std::string& next);

// The user's own library entry of a title, from a reply of
// library-entries; nothing when the reply has none.
[[nodiscard]] web::answer<std::optional<web::siteEntry>> parseEntry(const std::string& body);
// The attributes of a library entry, as Catalist's lists have the title.
[[nodiscard]] std::string libraryAttributes(const entry& title, const web::sending& what = {});

class tracker : public web::iTracker {
public:
	explicit tracker(web::link net) : net_(std::move(net)) {}

	[[nodiscard]] provider site() const override { return provider::kKitsu; }
	[[nodiscard]] web::answer<std::vector<web::match>> search(mediaType type,
	                                                          const std::string& query) override;
	[[nodiscard]] web::answer<web::details> lookUp(const entry& known) override;
	// OAuth 2's password grant, as Kitsu has it: the email and password are
	// sent once, for a token, and not kept.
	[[nodiscard]] web::answer<web::loginStart> startLogin() override;
	[[nodiscard]] web::answer<account> finishLogin(
	    const std::vector<std::string>& answers) override;
	[[nodiscard]] web::answer<account> current(const account& login) override;
	[[nodiscard]] web::answer<std::vector<entry>> pull(mediaType type,
	                                                   const std::string& user) override;
	[[nodiscard]] web::answer<std::optional<web::siteEntry>> entryOf(
	    const entry& title, const account& login) override;
	[[nodiscard]] web::answer<void> push(const entry& title, const account& login,
	                                     const web::sending& what) override;
	[[nodiscard]] web::answer<void> remove(mediaType type, int siteId,
	                                       const account& login) override;

protected:
	void pause() const override { net_.pause(); }

private:
	// The reply to a request, or to a GET; the failure when Kitsu cannot be
	// reached or said no.
	[[nodiscard]] web::answer<std::string> send(const web::request& asked) const;
	[[nodiscard]] web::answer<std::string> get(const std::string& url) const;

	// The token request with these fields, and the program's id when it has
	// one; the account it is for.
	[[nodiscard]] web::answer<account> accountFor(
	    std::vector<std::pair<std::string, std::string>> fields);

	// The id of the account's library entry for the title; 0 when it has
	// none.
	[[nodiscard]] web::answer<int> libraryEntry(mediaType type, int kitsuId,
	                                            const account& login) const;
	// The headers of a request made as the account.
	[[nodiscard]] static std::vector<std::string> asUser(const account& login);

	web::link net_;
};

}  // namespace catalist::kitsu

#endif  // CATALIST_WEB_KITSU_HPP_
