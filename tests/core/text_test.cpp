#include "core/text.hpp"

#include <doctest/doctest.h>

#include <string>

namespace utf8 = catalist::utf8;

TEST_CASE("utf8 width counts the columns a terminal gives the text") {
	CHECK(utf8::Width("") == 0);
	CHECK(utf8::Width("Odd Taxi") == 8);
	// Accents are one column, whether one code point or a letter and a mark.
	CHECK(utf8::Width("Pokémon") == 7);
	CHECK(utf8::Width("Pokémon") == 7);
	// Kana, kanji, hangul and full-width forms take two.
	CHECK(utf8::Width("葬送のフリーレン") == 16);
	CHECK(utf8::Width("나 혼자만 레벨업") == 16);
	CHECK(utf8::Width("ＡＢ") == 4);
	// Half-width katakana take one.
	CHECK(utf8::Width("ｱｲ") == 2);
	CHECK(utf8::Width("🎉") == 2);
	CHECK(utf8::Width("…") == 1);
}

TEST_CASE("utf8 fit clips at a whole glyph and pads to the width") {
	CHECK(utf8::Fit("Odd Taxi", 10) == "Odd Taxi  ");
	CHECK(utf8::Fit("Sousou no Frieren", 8) == "Sousou …");
	// A wide glyph that would cross the edge is left out, and a space keeps
	// the width.
	CHECK(utf8::Fit("葬送のフリーレン", 6) == "葬送… ");
	CHECK(utf8::Width(utf8::Fit("葬送のフリーレン", 6)) == 6);
}

TEST_CASE("utf8 clip keeps whole glyphs") {
	CHECK(utf8::Clip("葬送のフリーレン", 2) == "葬送");
	CHECK(utf8::Clip("short", 10) == "short");
	// A sequence cut short at the end is kept as it is.
	CHECK(utf8::Clip(std::string("ab\xE3\x81"), 3) == std::string("ab\xE3\x81"));
}

TEST_CASE("utf8 folded ignores case and punctuation") {
	CHECK(utf8::Folded("Frieren: Beyond Journey's End") == "frieren beyond journeys end");
	CHECK(utf8::Folded("  Re:ZERO -Starting Life-  ") == "re zero starting life");
	CHECK(utf8::Folded("Journey\u2019s \u2014 End") == "journeys end");
	CHECK(utf8::Folded("HUNTER×HUNTER") == "hunter x hunter");
	CHECK(utf8::Folded("葬送のフリーレン") == "葬送のフリーレン");
	CHECK(utf8::Folded("!?-").empty());
	CHECK(utf8::Words("Odd, Taxi") == std::vector<std::string>{"odd", "taxi"});
	CHECK(utf8::Words("--").empty());
}

TEST_CASE("utf8 edit distance counts a swap as one edit") {
	CHECK(utf8::EditDistance("taxi", "taxi") == 0);
	CHECK(utf8::EditDistance("tiax", "taxi") == 2);
	CHECK(utf8::EditDistance("freiren", "frieren") == 1);
	CHECK(utf8::EditDistance("", "abc") == 3);
	CHECK(utf8::EditDistance("フリーレン", "フリレン") == 1);
}
