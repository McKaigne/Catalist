#ifndef CATALIST_CORE_RESULT_HPP_
#define CATALIST_CORE_RESULT_HPP_

#include <expected>
#include <string>

namespace catalist {

template <typename T>
using Result = std::expected<T, std::string>;

}  // namespace catalist

#endif  // CATALIST_CORE_RESULT_HPP_
