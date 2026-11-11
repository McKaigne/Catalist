#ifndef CATALIST_TESTS_SUPPORT_SAMPLE_LIBRARY_HPP_
#define CATALIST_TESTS_SUPPORT_SAMPLE_LIBRARY_HPP_

#include "catalogue.hpp"

namespace catalist::test {

// A small fixed library with the awkward cases: long titles, quotes and
// commas, unknown totals, unrated entries and manga volumes.
// Anime: Sousou no Frieren (10), Odd Taxi (8), Ghost, "in" the Shell (6) and
// the long title (unrated). Manga: Berserk (10) and Chainsaw Man (9).
void seedSampleLibrary(catalogue& db);

}  // namespace catalist::test

#endif  // CATALIST_TESTS_SUPPORT_SAMPLE_LIBRARY_HPP_
