#include <doctest/doctest.h>

#include <filesystem>

#include "catalist/core/logger.hpp"
#include "support/testSupport.hpp"

using catalist::test::tempDirFixture;

TEST_CASE_FIXTURE(tempDirFixture, "logger writes exactly one formatted line") {
  auto logPath = tempDir() / "catalist.log";
  std::filesystem::remove(logPath);

  auto clock = catalist::test::makeTestClock();
  catalist::logger::get().init(logPath, clock);
  LOG_INFO("Testing logger output line");

  REQUIRE(std::filesystem::exists(logPath));
  auto content = catalist::test::readTextFile(logPath);
  CHECK(content ==
        "[2026-10-20 12:00:00.000] [INFO ] Testing logger output line\n");
}
