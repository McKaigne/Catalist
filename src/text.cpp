#include "text.hpp"

#include <algorithm>
#include <cctype>
#include <ftxui/screen/string.hpp>

namespace catalist::utf8 {

namespace {

size_t utf8SequenceLength(unsigned char lead) {
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

}  // namespace

std::vector<std::string> splitGlyphs(const std::string& text) {
	std::vector<std::string> glyphs;
	size_t position = 0;
	while (position < text.size()) {
		size_t length = utf8SequenceLength(static_cast<unsigned char>(text[position]));
		length = std::min(length, text.size() - position);
		glyphs.push_back(text.substr(position, length));
		position += length;
	}
	return glyphs;
}

std::string clip(const std::string& text, size_t maxGlyphs) {
	auto glyphs = splitGlyphs(text);
	if (glyphs.size() <= maxGlyphs)
		return text;
	std::string clipped;
	for (size_t i = 0; i < maxGlyphs; ++i)
		clipped += glyphs[i];
	return clipped;
}

int width(const std::string& text) {
	return ftxui::string_width(text);
}

std::string fit(const std::string& text, int columns) {
	int used = width(text);
	if (used <= columns)
		return text + std::string(static_cast<size_t>(columns - used), ' ');
	// Leave a column for the ellipsis.
	std::string fitted;
	used = 0;
	for (const auto& glyph : splitGlyphs(text)) {
		int glyphWidth = width(glyph);
		if (used + glyphWidth > columns - 1)
			break;
		fitted += glyph;
		used += glyphWidth;
	}
	return fitted + "…" + std::string(static_cast<size_t>(std::max(0, columns - 1 - used)), ' ');
}

std::string lowercase(std::string text) {
	std::transform(text.begin(), text.end(), text.begin(),
	               [](unsigned char c) { return std::tolower(c); });
	return text;
}

}  // namespace catalist::utf8
