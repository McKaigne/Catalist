#ifndef CATALIST_CORE_URL_HPP_
#define CATALIST_CORE_URL_HPP_

#include <optional>
#include <string>
#include <string_view>

namespace catalist::url {

// RFC 3986 percent-encoding; only unreserved characters pass through, so the
// result is safe in a query string and in an x-www-form-urlencoded body.
[[nodiscard]] std::string encode(std::string_view text);

// Decodes %XX escapes and '+' as space. Malformed escapes make it fail rather
// than guess.
[[nodiscard]] std::optional<std::string> decode(std::string_view text);

}  // namespace catalist::url

#endif  // CATALIST_CORE_URL_HPP_
