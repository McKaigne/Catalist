#ifndef CATALIST_JIKAN_HPP_
#define CATALIST_JIKAN_HPP_

#include <string>
#include <vector>

#include "entry.hpp"
#include "result.hpp"
#include "system.hpp"

// Title search on Jikan, the public read-only API over MyAnimeList. It needs
// no account and is the only thing Catalist uses the network for.
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
[[nodiscard]] result<std::vector<title>> parseSearch(const std::string& body,
                                                     mediaType type);

// Up to ten matches, best first. Fails when Jikan cannot be reached, which
// the caller must tell apart from a search that simply found nothing.
[[nodiscard]] result<std::vector<title>> search(iHttpClient& http,
                                                mediaType type,
                                                const std::string& query);

}  // namespace catalist::jikan

#endif  // CATALIST_JIKAN_HPP_
