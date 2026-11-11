#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "catalist/tui/theme/palettes.hpp"
#include "catalist/tui/theme/statusStyle.hpp"
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

TEST_CASE("tui star meter shows two points a star, halves included") {
  using catalist::renderStarMeter;
  namespace icon = catalist::icon;
  auto stars = [](int full, int half, int empty) {
    std::vector<std::string> parts(full, icon::kStarFull);
    parts.insert(parts.end(), half, icon::kStarHalf);
    parts.insert(parts.end(), empty, icon::kStarEmpty);
    std::string joined;
    for (const auto& part : parts)
      joined += (joined.empty() ? "" : " ") + part;
    return joined;
  };
  CHECK(renderStarMeter(0) == stars(0, 0, 5));
  CHECK(renderStarMeter(1) == stars(0, 1, 4));
  CHECK(renderStarMeter(7) == stars(3, 1, 1));
  CHECK(renderStarMeter(10) == stars(5, 0, 0));
  CHECK(renderStarMeter(-3) == stars(0, 0, 5));
  CHECK(renderStarMeter(15) == stars(5, 0, 0));
}
