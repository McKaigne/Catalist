#include <doctest/doctest.h>

#include <filesystem>
#include <regex>

#include "catalist/core/logger.hpp"
#include "support/testSupport.hpp"

using catalist::test::tempDirFixture;

TEST_CASE_FIXTURE(tempDirFixture, "logger writes exactly one formatted line") {
  auto logPath = tempDir() / "catalist.log";
  std::filesystem::remove(logPath);

  catalist::Logger::get().init(logPath);
  LOG_INFO("Testing logger output line");

  REQUIRE(std::filesystem::exists(logPath));
  auto content = catalist::test::readTextFile(logPath);
  // The timestamp is wall-clock time, so it is matched by shape only.
  std::regex expected(
      R"(\[\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3}\] \[INFO \] Testing logger output line\n)");
  CHECK_MESSAGE(std::regex_match(content, expected), "log content: ", content);
}
