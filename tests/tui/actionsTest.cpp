#include <doctest/doctest.h>

#include "catalist/tui/actions.hpp"
#include "tui/tuiHarness.hpp"

TEST_CASE("tui aboutLine summarises version counts and data dir") {
  catalist::test::tuiHarness harness;
  auto line = catalist::aboutLine(harness.ctx);
  CHECK(line.find("Catalist v") == 0);
  CHECK(line.find("0 anime · 0 manga · data in /data") != std::string::npos);
}
