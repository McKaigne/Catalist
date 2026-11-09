#include <doctest/doctest.h>

#include <cctype>
#include <filesystem>
#include <string>

#include "catalist/core/logger.hpp"
#include "support/testSupport.hpp"

using catalist::test::tempDirFixture;

namespace {

// The timestamp is wall-clock time, so only its shape is checked:
// "[YYYY-MM-DD HH:MM:SS.mmm] ".
bool hasTimestampPrefix(const std::string& line) {
  const std::string shape = "[dddd-dd-dd dd:dd:dd.ddd] ";
  if (line.size() < shape.size())
    return false;
  for (size_t i = 0; i < shape.size(); ++i) {
    bool wantsDigit = shape[i] == 'd';
    bool isDigit = std::isdigit(static_cast<unsigned char>(line[i])) != 0;
    if (wantsDigit != isDigit)
      return false;
    if (!wantsDigit && line[i] != shape[i])
      return false;
  }
  return true;
}

}  // namespace

TEST_CASE_FIXTURE(tempDirFixture, "logger writes exactly one formatted line") {
  auto logPath = tempDir() / "catalist.log";
  std::filesystem::remove(logPath);

  catalist::Logger::get().init(logPath);
  LOG_INFO("Testing logger output line");

  REQUIRE(std::filesystem::exists(logPath));
  auto content = catalist::test::readTextFile(logPath);
  CAPTURE(content);
  const std::string timestampWidth = "[0000-00-00 00:00:00.000] ";
  REQUIRE(content.size() > timestampWidth.size());
  CHECK(hasTimestampPrefix(content));
  CHECK(content.substr(timestampWidth.size()) ==
        "[INFO ] Testing logger output line\n");
}
