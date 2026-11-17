#ifndef CATALIST_WEB_SITES_HPP_
#define CATALIST_WEB_SITES_HPP_

#include <memory>

#include "core/entry.hpp"
#include "web/tracker.hpp"

namespace catalist::web {

// The adapter of a site: AniList, MyAnimeList or Kitsu, all one interface.
[[nodiscard]] std::unique_ptr<Tracker> MakeTracker(Provider site, const Link& net);

}  // namespace catalist::web

#endif  // CATALIST_WEB_SITES_HPP_
