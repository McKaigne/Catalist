#include "core/text.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <iterator>

namespace catalist::utf8 {

namespace {

size_t Utf8SequenceLength(unsigned char lead) {
	if ((lead & 0x80) == 0)
		return 1;
	if ((lead & 0xE0) == 0xC0)
		return 2;
	if ((lead & 0xF0) == 0xE0)
		return 3;
	if ((lead & 0xF8) == 0xF0)
		return 4;
	return 1;
}

// The code point of one glyph of SplitGlyphs; a broken sequence counts as
// one ordinary character.
std::uint32_t CodePoint(const std::string& glyph) {
	auto lead = static_cast<unsigned char>(glyph[0]);
	if (glyph.size() == 1)
		return lead;
	std::uint32_t point = lead & (0xFF >> (glyph.size() + 1));
	for (size_t i = 1; i < glyph.size(); ++i)
		point = (point << 6) | (static_cast<unsigned char>(glyph[i]) & 0x3F);
	return point;
}

struct Range {
	std::uint32_t first;
	std::uint32_t last;
};

bool InRanges(std::uint32_t point, const Range* ranges, size_t count) {
	return std::any_of(ranges, ranges + count,
	                   [point](const Range& r) { return point >= r.first and point <= r.last; });
}

// Marks drawn over the glyph before them, and characters that are not drawn:
// they take no column.
constexpr Range kZeroWidth[] = {
    {0x0000, 0x001F}, {0x007F, 0x009F}, {0x0300, 0x036F}, {0x1AB0, 0x1AFF},   {0x1DC0, 0x1DFF},
    {0x200B, 0x200F}, {0x2028, 0x202E}, {0x2060, 0x2064}, {0x20D0, 0x20FF},   {0x3099, 0x309A},
    {0xFE00, 0xFE0F}, {0xFE20, 0xFE2F}, {0xFEFF, 0xFEFF}, {0xE0100, 0xE01EF},
};

// East Asian wide and full-width characters, and emoji: two columns each
// (Unicode's East Asian Width, as terminals apply it).
constexpr Range kDoubleWidth[] = {
    {0x1100, 0x115F},   {0x231A, 0x231B},   {0x2329, 0x232A},   {0x23E9, 0x23EC},
    {0x25FD, 0x25FE},   {0x2614, 0x2615},   {0x2648, 0x2653},   {0x26AA, 0x26AB},
    {0x26BD, 0x26BE},   {0x26C4, 0x26C5},   {0x2705, 0x2705},   {0x270A, 0x270B},
    {0x2753, 0x2755},   {0x2795, 0x2797},   {0x2B1B, 0x2B1C},   {0x2E80, 0x303E},
    {0x3041, 0x3098},   {0x309B, 0x33FF},   {0x3400, 0x4DBF},   {0x4E00, 0x9FFF},
    {0xA000, 0xA4CF},   {0xA960, 0xA97F},   {0xAC00, 0xD7A3},   {0xF900, 0xFAFF},
    {0xFE10, 0xFE19},   {0xFE30, 0xFE6F},   {0xFF00, 0xFF60},   {0xFFE0, 0xFFE6},
    {0x1F300, 0x1F64F}, {0x1F680, 0x1F6FF}, {0x1F900, 0x1F9FF}, {0x20000, 0x2FFFD},
    {0x30000, 0x3FFFD},
};

int GlyphWidth(const std::string& glyph) {
	std::uint32_t point = CodePoint(glyph);
	if (InRanges(point, kZeroWidth, std::size(kZeroWidth)))
		return 0;
	return InRanges(point, kDoubleWidth, std::size(kDoubleWidth)) ? 2 : 1;
}

}  // namespace

std::vector<std::string> SplitGlyphs(const std::string& text) {
	std::vector<std::string> glyphs;
	size_t position = 0;
	while (position < text.size()) {
		size_t length = Utf8SequenceLength(static_cast<unsigned char>(text[position]));
		length = std::min(length, text.size() - position);
		glyphs.push_back(text.substr(position, length));
		position += length;
	}
	return glyphs;
}

std::string Clip(const std::string& text, size_t max_glyphs) {
	auto glyphs = SplitGlyphs(text);
	if (glyphs.size() <= max_glyphs)
		return text;
	std::string clipped;
	for (size_t i = 0; i < max_glyphs; ++i)
		clipped += glyphs[i];
	return clipped;
}

int Width(const std::string& text) {
	int columns = 0;
	for (const auto& glyph : SplitGlyphs(text))
		columns += GlyphWidth(glyph);
	return columns;
}

std::string Fit(const std::string& text, int columns) {
	int used = Width(text);
	if (used <= columns)
		return text + std::string(static_cast<size_t>(columns - used), ' ');
	// Leave a column for the ellipsis.
	std::string fitted;
	used = 0;
	for (const auto& glyph : SplitGlyphs(text)) {
		int columns_taken = GlyphWidth(glyph);
		if (used + columns_taken > columns - 1)
			break;
		fitted += glyph;
		used += columns_taken;
	}
	return fitted + "…" + std::string(static_cast<size_t>(std::max(0, columns - 1 - used)), ' ');
}

std::string Lowercase(std::string text) {
	for (char& c : text) {
		if (c >= 'A' and c <= 'Z')
			c = static_cast<char>(c + ('a' - 'A'));
	}
	return text;
}

}  // namespace catalist::utf8
