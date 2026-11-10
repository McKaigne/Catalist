#ifndef CATALIST_CORE_URL_HPP_
#define CATALIST_CORE_URL_HPP_

#include <string>
#include <string_view>

namespace catalist::url {

// RFC 3986 percent-encoding; only unreserved characters pass through, so the
// result is safe in a query string.
[[nodiscard]] std::string encode(std::string_view text);

}  // namespace catalist::url

#endif  // CATALIST_CORE_URL_HPP_
