#ifndef CATALIST_CORE_TEXT_HPP_
#define CATALIST_CORE_TEXT_HPP_

#include <string>
#include <vector>

namespace catalist::utf8 {

[[nodiscard]] std::vector<std::string> SplitGlyphs(const std::string& text);

[[nodiscard]] std::string Clip(const std::string& text, size_t max_glyphs);

[[nodiscard]] int Width(const std::string& text);

[[nodiscard]] std::string Fit(const std::string& text, int columns);

[[nodiscard]] std::string Lowercase(std::string text);

// For matching names: lower case, apostrophes gone, other punctuation a
// single space.
[[nodiscard]] std::string Folded(const std::string& text);
[[nodiscard]] std::vector<std::string> Words(const std::string& text);

// Edits between two texts, in glyphs; swapping two neighbours is one edit.
[[nodiscard]] int EditDistance(const std::string& a, const std::string& b);

}  // namespace catalist::utf8

#endif  // CATALIST_CORE_TEXT_HPP_
