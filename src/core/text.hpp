#ifndef CATALIST_CORE_TEXT_HPP_
#define CATALIST_CORE_TEXT_HPP_

#include <string>
#include <vector>

// UTF-8 text as it shows on a terminal: titles are often Japanese, so bytes
// are not glyphs and glyphs are not columns.
namespace catalist::utf8 {

// Splits UTF-8 text into code points so clipping never cuts a glyph in half.
[[nodiscard]] std::vector<std::string> splitGlyphs(const std::string& text);

// The first maxGlyphs glyphs.
[[nodiscard]] std::string clip(const std::string& text, size_t maxGlyphs);

// How many terminal columns the text takes (a CJK glyph takes two).
[[nodiscard]] int width(const std::string& text);

// Exactly `columns` wide: clipped with "…" when longer, padded with spaces
// when shorter.
[[nodiscard]] std::string fit(const std::string& text, int columns);

// ASCII lower case, byte by byte, for matching titles without regard to case.
[[nodiscard]] std::string lowercase(std::string text);

}  // namespace catalist::utf8

#endif  // CATALIST_CORE_TEXT_HPP_
