#ifndef CATALIST_WEB_SCORES_HPP_
#define CATALIST_WEB_SCORES_HPP_

#include <string>

#include "core/entry.hpp"

// Scores as each site shows them to the person, in the format they chose
// there, as Mihon's trackers display them. Pure text out.
namespace catalist::web {

// The score `raw`, as the site keeps it (AniList out of 100, MyAnimeList out
// of 10, Kitsu's ratingTwenty from 2 to 20), in the person's `format` on that
// site (see account::scoreFormat): "8.5", or stars as "4/5"; "-" for no
// score. The faces some formats draw are shown as stars on their scale.
[[nodiscard]] std::string displayScore(provider site, const std::string& format, double raw);

}  // namespace catalist::web

#endif  // CATALIST_WEB_SCORES_HPP_
