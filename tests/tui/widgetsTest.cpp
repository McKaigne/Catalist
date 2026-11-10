#include <doctest/doctest.h>

#include "catalist/tui/widgets/marquee.hpp"

TEST_CASE("tui marquee clips by glyph, not byte") {
  CHECK(catalist::marquee::clip("葬送のフリーレン", 3) == "葬送の");
  CHECK(catalist::marquee::clip("short", 10) == "short");
  CHECK(catalist::marquee::splitGlyphs("aé葬").size() == 3);
}

TEST_CASE("tui marquee scrolls after the pause and loops") {
  using catalist::marquee::scroll;
  CHECK(scroll("abcdef", 3, 0) == "abc");
  CHECK(scroll("abcdef", 3, 2000 + 250) == "bcd");
  CHECK(scroll("ab", 3, 99999) == "ab");
}
