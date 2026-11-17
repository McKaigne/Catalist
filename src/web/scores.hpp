#ifndef CATALIST_WEB_SCORES_HPP_
#define CATALIST_WEB_SCORES_HPP_

#include <string>

#include "core/entry.hpp"

// Scores as stars: every site's, and every format a person can score in
// there, shown the one way. Pure text out.
namespace catalist::web {

// A score out of 10 as five Nerd Font stars, each half star a point:
// 7 is three full stars, a half and an empty one. Rounded to the nearest
// point; "-" for no score.
[[nodiscard]] std::string stars(double outOfTen);

// The score `raw`, as `site` keeps it (AniList out of 100, MyAnimeList out of
// 10, Kitsu's ratingTwenty from 2 to 20), out of 10.
[[nodiscard]] double outOfTen(provider site, double raw);

}  // namespace catalist::web

#endif  // CATALIST_WEB_SCORES_HPP_
