#include <doctest/doctest.h>

#include <string>

#include "core/text.hpp"

namespace utf8 = catalist::utf8;

TEST_CASE("utf8 width counts the columns a terminal gives the text") {
	CHECK(utf8::width("") == 0);
	CHECK(utf8::width("Odd Taxi") == 8);
	// Accents are one column, whether one code point or a letter and a mark.
	CHECK(utf8::width("Pokémon") == 7);
	CHECK(utf8::width("Pokémon") == 7);
	// Kana, kanji, hangul and full-width forms take two.
	CHECK(utf8::width("葬送のフリーレン") == 16);
	CHECK(utf8::width("나 혼자만 레벨업") == 16);
	CHECK(utf8::width("ＡＢ") == 4);
	// Half-width katakana take one.
	CHECK(utf8::width("ｱｲ") == 2);
	CHECK(utf8::width("🎉") == 2);
	CHECK(utf8::width("…") == 1);
}

TEST_CASE("utf8 fit clips at a whole glyph and pads to the width") {
	CHECK(utf8::fit("Odd Taxi", 10) == "Odd Taxi  ");
	CHECK(utf8::fit("Sousou no Frieren", 8) == "Sousou …");
	// A wide glyph that would cross the edge is left out, and a space keeps
	// the width.
	CHECK(utf8::fit("葬送のフリーレン", 6) == "葬送… ");
	CHECK(utf8::width(utf8::fit("葬送のフリーレン", 6)) == 6);
}

TEST_CASE("utf8 clip keeps whole glyphs") {
	CHECK(utf8::clip("葬送のフリーレン", 2) == "葬送");
	CHECK(utf8::clip("short", 10) == "short");
	// A sequence cut short at the end is kept as it is.
	CHECK(utf8::clip(std::string("ab\xE3\x81"), 3) == std::string("ab\xE3\x81"));
}
