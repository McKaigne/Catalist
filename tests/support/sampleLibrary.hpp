#ifndef CATALIST_TESTS_SUPPORT_SAMPLE_LIBRARY_HPP_
#define CATALIST_TESTS_SUPPORT_SAMPLE_LIBRARY_HPP_

#include "catalist/storage/database.hpp"

namespace catalist::test {

// A small fixed library that exercises the CLI formatters' edge cases: long
// titles, quotes and commas, multiline notes, unknown totals, unrated entries
// and manga volumes.
void seedSampleLibrary(database& db);

}  // namespace catalist::test

#endif  // CATALIST_TESTS_SUPPORT_SAMPLE_LIBRARY_HPP_
