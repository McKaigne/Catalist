#ifndef CATALIST_CORE_JSON_LITE_HPP_
#define CATALIST_CORE_JSON_LITE_HPP_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Field scanners for the small, flat JSON payloads the remote APIs return.
// They find the first occurrence of a key anywhere in the text, so callers
// pass in the object block they care about rather than a whole document.
namespace catalist::jsonLite {

[[nodiscard]] std::optional<std::string> extractJsonString(
    std::string_view block, std::string_view key);

// Accepts bare and quoted numbers; a JSON null counts as absent.
[[nodiscard]] std::optional<double> extractJsonNumber(std::string_view block,
                                                      std::string_view key);

// Returns the text of each top-level object inside the array stored under
// arrayKey, e.g. every element of "data": [ {...}, {...} ].
[[nodiscard]] std::vector<std::string> splitArrayObjects(
    std::string_view json, std::string_view arrayKey);

[[nodiscard]] std::string unescapeJsonString(std::string_view text);
[[nodiscard]] std::string escapeJsonString(std::string_view text);

}  // namespace catalist::jsonLite

#endif  // CATALIST_CORE_JSON_LITE_HPP_
