#ifndef CATALIST_MODEL_SERVICE_LINKS_HPP_
#define CATALIST_MODEL_SERVICE_LINKS_HPP_

#include <string>
#include <vector>

#include "catalist/model/entry.hpp"

namespace catalist {

// The entry's page on the first service it has an id for (MyAnimeList, then
// AniList); empty when it has none.
[[nodiscard]] std::string webAddress(const entry& mediaEntry);

// "MAL #52991"-style labels for every service id the entry has.
[[nodiscard]] std::vector<std::string> serviceBadges(const entry& mediaEntry);

}  // namespace catalist

#endif  // CATALIST_MODEL_SERVICE_LINKS_HPP_
