#include <doctest/doctest.h>

#include <sstream>
#include <string>

#include "catalist/core/version.hpp"

namespace {

bool isUnsignedNumber(const std::string& text) {
  if (text.empty())
    return false;
  for (char c : text) {
    if (c < '0' || c > '9')
      return false;
  }
  return true;
}

}  // namespace

TEST_CASE("kVersionString is major.minor.patch matching the numeric parts") {
  std::string version = catalist::kVersionString;
  std::stringstream parts(version);
  std::string major;
  std::string minor;
  std::string patch;
  std::string extra;
  REQUIRE(std::getline(parts, major, '.'));
  REQUIRE(std::getline(parts, minor, '.'));
  REQUIRE(std::getline(parts, patch, '.'));
  CHECK_FALSE(std::getline(parts, extra, '.'));

  REQUIRE(isUnsignedNumber(major));
  REQUIRE(isUnsignedNumber(minor));
  REQUIRE(isUnsignedNumber(patch));
  CHECK(std::stoi(major) == catalist::kVersionMajor);
  CHECK(std::stoi(minor) == catalist::kVersionMinor);
  CHECK(std::stoi(patch) == catalist::kVersionPatch);
}
