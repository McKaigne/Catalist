#ifndef CATALIST_JIKAN_HPP_
#define CATALIST_JIKAN_HPP_

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "entry.hpp"
#include "result.hpp"

// Jikan, the public read-only API over MyAnimeList: the title search, and
// what is known about one title (totals, how much is out, the cover). It
// needs no account.
namespace catalist::jikan {

struct title {
	int malId = 0;
	std::string name;
	// Episodes or chapters; 0 while the series is still running.
	int totalUnits = 0;
	int totalVolumes = 0;
	// Four digits, or empty when MyAnimeList has no date.
	std::string year;
	// The site-wide score, 0 when there is none yet.
	double score = 0.0;
};

[[nodiscard]] std::string searchUrl(mediaType type, const std::string& query);

// Reads a search reply; fails when the text is not one.
[[nodiscard]] result<std::vector<title>> parseSearch(const std::string& body, mediaType type);

// Fetches a URL: the body of the reply, or nothing when the request failed.
// The program passes httpGet; tests pass a fake.
using fetcher = std::function<std::optional<std::string>(const std::string&)>;

// Up to ten matches, best first. Fails when Jikan cannot be reached, which
// the caller must tell apart from a search that simply found nothing.
[[nodiscard]] result<std::vector<title>> search(const fetcher& fetch, mediaType type,
                                                const std::string& query);

// What MyAnimeList says about one title.
struct details {
	// released: everything, once it has finished; for an anime still airing,
	// the episodes aired so far (see lookUp); otherwise 0, unknown.
	webFacts facts;
	// Still airing or publishing.
	bool ongoing = false;
	std::string imageUrl;
};

[[nodiscard]] std::string detailsUrl(mediaType type, int malId);
[[nodiscard]] std::string episodesUrl(int malId, int page);

// Reads a title's page; fails when the text is not one.
[[nodiscard]] result<details> parseDetails(const std::string& body, mediaType type);

// One page of an anime's episode list: how many pages there are and how
// many of this page's episodes had aired by `today`.
struct episodePage {
	int lastPage = 1;
	int aired = 0;
};
[[nodiscard]] result<episodePage> parseEpisodes(const std::string& body, const isoDate& today);

// The title's details; for an anime still airing, one or two more requests
// count the episodes aired so far (a hundred to a page). Fails when Jikan
// cannot be reached for the details.
[[nodiscard]] result<details> lookUp(const fetcher& fetch, mediaType type, int malId,
                                     const isoDate& today);

}  // namespace catalist::jikan

#endif  // CATALIST_JIKAN_HPP_
