#ifndef CATALIST_WEB_TRACKER_HPP_
#define CATALIST_WEB_TRACKER_HPP_

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "core/account.hpp"
#include "core/entry.hpp"
#include "core/result.hpp"
#include "web/http.hpp"

// A tracking site. Every site does the same jobs behind this one interface,
// so no command is tied to a site: the person's lists are the truth, and a
// site is asked about titles (and, later, told what changed in the lists,
// one way, as Mihon tells its trackers).
namespace catalist::web {

// The ids the sites gave this program, for their APIs and their logins.
// MyAnimeList answers nothing without its id; AniList and Kitsu need theirs
// only to log in.
struct appIds {
	std::string anilist{};
	std::string mal{};
	std::string kitsu{};
	std::string kitsuSecret{};
};

// What a site is reached through. The program passes the network; the tests
// a fake.
struct link {
	fetcher fetch;
	// The wait between two requests to one site, so that none is flooded.
	std::function<void()> pause = [] {};
	appIds ids{};
	// Seconds since 1970, for when a login runs out.
	std::function<long long()> now = [] { return 0LL; };
	// Random text of the length asked, for OAuth's PKCE.
	std::function<std::string(size_t)> random = [](size_t length) {
		return std::string(length, 'x');
	};
};

// What to ask of the person to log in: a page to open (empty when there is
// none), then the answers to read, in order.
struct question {
	std::string ask;
	// Not shown as it is typed: a password.
	bool secret = false;
};
struct loginStart {
	std::string page;
	std::string explanation;
	std::vector<question> questions;
};

// One match of a search on a site.
struct match {
	// The ids the site knows the title by; 0 for those it does not.
	int anilistId = 0;
	int malId = 0;
	int kitsuId = 0;
	std::string name;
	// Episodes or chapters; 0 while the series is still running.
	int totalUnits = 0;
	int totalVolumes = 0;
	// Four digits, or empty when there is no date.
	std::string year;
	// The site-wide score out of 10, 0 when there is none yet.
	double score = 0.0;
};

// What a site knows about one title.
struct details {
	std::string name;
	// The ids, totals and, once the title has ended, everything as released;
	// for an anime still airing, the episodes aired so far where the site
	// says so.
	webFacts facts;
	// Still airing or publishing.
	bool ongoing = false;
};

class iTracker {
public:
	virtual ~iTracker() = default;

	[[nodiscard]] virtual provider site() const = 0;

	// Up to ten matches, best first. Fails when the site cannot be reached,
	// which the caller must tell apart from a search that found nothing.
	[[nodiscard]] virtual result<std::vector<match>> search(mediaType type,
	                                                        const std::string& query) = 0;

	// The title by the site's own id, else by its MyAnimeList id. Fails when
	// it has neither, when the site does not know it, or cannot be reached.
	[[nodiscard]] virtual result<details> lookUp(const entry& known) = 0;

	// What lookUp says of each title, in the order given. Here one request
	// a title, a pause apart; a site that answers many in one request does
	// better. Once the site cannot be reached, every title left fails so.
	[[nodiscard]] virtual std::vector<result<details>> lookUpMany(const std::vector<entry>& known);

	// ---- Logging in ----

	// The first half: what to show and ask the person. Fails when this
	// build has no id from the site to log in with.
	[[nodiscard]] virtual result<loginStart> startLogin() = 0;
	// The second half: the answers, in the order asked, for the account.
	[[nodiscard]] virtual result<account> finishLogin(const std::vector<std::string>& answers) = 0;
	// The account with a token that has not run out: as it is while it has
	// not, else renewed. Fails when it cannot be, and the person must log in
	// again.
	[[nodiscard]] virtual result<account> current(const account& login) = 0;

protected:
	// The pause between two requests of lookUpMany.
	virtual void pause() const = 0;
};

// "could not reach AniList": the site is not there, as opposed to a title
// it does not know.
[[nodiscard]] bool unreachable(const std::string& message);

// What an OAuth token endpoint answers: the token, what renews it, and how
// many seconds it lasts. Nothing when the reply has no token.
struct tokenReply {
	std::string token;
	std::string renewal;
	long long lifetime = 0;
};
[[nodiscard]] std::optional<tokenReply> parseTokenReply(const std::string& body);

// "Authorization: Bearer <token>".
[[nodiscard]] std::string bearer(const std::string& token);

// Whether the login's token runs out within a minute (or has).
[[nodiscard]] bool runningOut(const account& login, long long now);

}  // namespace catalist::web

#endif  // CATALIST_WEB_TRACKER_HPP_
