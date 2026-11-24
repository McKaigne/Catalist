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

constexpr Range kPunctuation[] = {
    {0x00A0, 0x00BF}, {0x2000, 0x206F}, {0x3000, 0x3004}, {0x3008, 0x3020}, {0x3030, 0x3030},
    {0x30FB, 0x30FB}, {0xFF01, 0xFF0F}, {0xFF1A, 0xFF20}, {0xFF3B, 0xFF40}, {0xFF5B, 0xFF65},
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

std::string Folded(const std::string& text) {
	std::string folded;
	auto gap = [&folded] {
		if (not folded.empty() and folded.back() != ' ')
			folded += ' ';
	};
	for (const auto& glyph : SplitGlyphs(text)) {
		std::uint32_t point = CodePoint(glyph);
		if (point == '\'' or point == '`' or point == 0x2018 or point == 0x2019)
			continue;
		// AniList writes the x of HUNTER x HUNTER as a multiplication sign.
		if (point == 0xD7) {
			gap();
			folded += "x ";
		} else if ((point < 0x80 and not std::isalnum(static_cast<unsigned char>(point))) or
		           InRanges(point, kPunctuation, std::size(kPunctuation))) {
			gap();
		} else {
			folded += Lowercase(glyph);
		}
	}
	if (not folded.empty() and folded.back() == ' ')
		folded.pop_back();
	return folded;
}

std::vector<std::string> Words(const std::string& text) {
	std::vector<std::string> words;
	std::string folded = Folded(text);
	size_t start = 0;
	while (start < folded.size()) {
		size_t end = std::min(folded.find(' ', start), folded.size());
		words.push_back(folded.substr(start, end - start));
		start = end + 1;
	}
	return words;
}

int EditDistance(const std::string& a, const std::string& b) {
	auto from = SplitGlyphs(a);
	auto to = SplitGlyphs(b);
	std::vector<std::vector<int>> cost(from.size() + 1, std::vector<int>(to.size() + 1));
	for (size_t i = 0; i <= from.size(); ++i)
		cost[i][0] = static_cast<int>(i);
	for (size_t j = 0; j <= to.size(); ++j)
		cost[0][j] = static_cast<int>(j);
	for (size_t i = 1; i <= from.size(); ++i) {
		for (size_t j = 1; j <= to.size(); ++j) {
			int swap = from[i - 1] == to[j - 1] ? 0 : 1;
			cost[i][j] =
			    std::min({cost[i - 1][j] + 1, cost[i][j - 1] + 1, cost[i - 1][j - 1] + swap});
			if (i > 1 and j > 1 and from[i - 1] == to[j - 2] and from[i - 2] == to[j - 1])
				cost[i][j] = std::min(cost[i][j], cost[i - 2][j - 2] + 1);
		}
	}
	return cost[from.size()][to.size()];
}

}  // namespace catalist::utf8
