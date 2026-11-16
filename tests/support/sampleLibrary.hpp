#ifndef CATALIST_TESTS_SUPPORT_SAMPLE_LIBRARY_HPP_
#define CATALIST_TESTS_SUPPORT_SAMPLE_LIBRARY_HPP_

#include <vector>

#include "core/entry.hpp"

namespace catalist::test {

// A small fixed library with the awkward cases: long titles, quotes and
// commas, unknown totals, unrated entries and manga volumes.
// Anime: Sousou no Frieren (10), Odd Taxi (8; 5 of 13 seen, all 13 out, so
// 8 new), the long title (unrated) and Ghost, "in" the Shell (6). Manga:
// Berserk (10) and Chainsaw Man (9).
[[nodiscard]] std::vector<entry> sampleList(mediaType type);

}  // namespace catalist::test

#endif  // CATALIST_TESTS_SUPPORT_SAMPLE_LIBRARY_HPP_
