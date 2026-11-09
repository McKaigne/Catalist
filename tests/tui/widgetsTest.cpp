#include <doctest/doctest.h>

#include "catalist/tui/widgets/marquee.hpp"
#include "catalist/tui/widgets/notifier.hpp"

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

TEST_CASE("tui notifier keeps status and queues popups in order") {
  catalist::notifier notices;
  CHECK(notices.status() == "Ready.");
  notices.setStatus("Busy");
  CHECK(notices.status() == "Busy");
  notices.push("first");
  notices.push("second");
  REQUIRE(notices.hasPopup());
  notices.dismiss();
  REQUIRE(notices.hasPopup());
  notices.dismiss();
  CHECK_FALSE(notices.hasPopup());
}
