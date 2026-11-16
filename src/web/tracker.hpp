#ifndef CATALIST_WEB_TRACKER_HPP_
#define CATALIST_WEB_TRACKER_HPP_

#include <functional>
#include <string>
#include <vector>

#include "core/entry.hpp"
#include "core/result.hpp"
#include "web/http.hpp"

// A tracking site. Every site does the same jobs behind this one interface,
// so no command is tied to a site: the person's lists are the truth, and a
// site is asked about titles (and, later, told what changed in the lists,
// one way, as Mihon tells its trackers).
namespace catalist::web {

// What a site is reached through. The program passes the network; the tests
// a fake.
struct link {
	fetcher fetch;
	// The wait between two requests to one site, so that none is flooded.
	std::function<void()> pause = [] {};
	// MyAnimeList's id for this program; its API answers nothing without.
	std::string malClientId{};
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

protected:
	// The pause between two requests of lookUpMany.
	virtual void pause() const = 0;
};

// "could not reach AniList": the site is not there, as opposed to a title
// it does not know.
[[nodiscard]] bool unreachable(const std::string& message);

}  // namespace catalist::web

#endif  // CATALIST_WEB_TRACKER_HPP_
