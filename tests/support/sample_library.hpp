#ifndef CATALIST_TESTS_SUPPORT_SAMPLE_LIBRARY_HPP_
#define CATALIST_TESTS_SUPPORT_SAMPLE_LIBRARY_HPP_

#include <vector>

#include "core/entry.hpp"

namespace catalist::test {

[[nodiscard]] std::vector<Entry> SampleList(MediaType type);

}  // namespace catalist::test

#endif  // CATALIST_TESTS_SUPPORT_SAMPLE_LIBRARY_HPP_
