#ifndef CATALIST_CORE_RESULT_HPP_
#define CATALIST_CORE_RESULT_HPP_

#include <expected>
#include <string>

namespace catalist {

// A value, or a message saying why there is none.
template <typename T>
using result = std::expected<T, std::string>;

// Work that has no value to give: done, or the message saying why not.
using done = std::expected<void, std::string>;

}  // namespace catalist

#endif  // CATALIST_CORE_RESULT_HPP_
