#ifndef CATALIST_CORE_TEXT_HPP_
#define CATALIST_CORE_TEXT_HPP_

#include <string>
#include <vector>

// UTF-8 text as it shows on a terminal: titles are often Japanese, so bytes
// are not glyphs and glyphs are not columns.
namespace catalist::utf8 {

// Splits UTF-8 text into code points so clipping never cuts a glyph in half.
[[nodiscard]] std::vector<std::string> SplitGlyphs(const std::string& text);

// The first max_glyphs glyphs.
[[nodiscard]] std::string Clip(const std::string& text, size_t max_glyphs);

// How many terminal columns the text takes (a CJK glyph takes two).
[[nodiscard]] int Width(const std::string& text);

// Exactly `columns` wide: clipped with "…" when longer, padded with spaces
// when shorter.
[[nodiscard]] std::string Fit(const std::string& text, int columns);

// ASCII lower case, byte by byte, for matching titles without regard to case.
[[nodiscard]] std::string Lowercase(std::string text);

// For matching names: lower case, apostrophes gone, other punctuation a
// single space.
[[nodiscard]] std::string Folded(const std::string& text);
[[nodiscard]] std::vector<std::string> Words(const std::string& text);

// Edits between two texts, in glyphs; swapping two neighbours is one edit.
[[nodiscard]] int EditDistance(const std::string& a, const std::string& b);

}  // namespace catalist::utf8

#endif  // CATALIST_CORE_TEXT_HPP_
