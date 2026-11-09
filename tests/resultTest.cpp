#include <doctest/doctest.h>

#include "catalist/core/result.hpp"

using catalist::Result;

TEST_CASE("result success holds the value") {
  auto okResult = Result<int>::success(42);
  CHECK(okResult.ok());
  CHECK(okResult.value() == 42);
}

TEST_CASE("result failure holds the message") {
  auto failResult = Result<int>::failure("Network timeout");
  CHECK_FALSE(failResult.ok());
  CHECK(failResult.errorMessage() == "Network timeout");
}
