#include <doctest/doctest.h>

#include "catalist/core/result.hpp"

using catalist::result;

TEST_CASE("result success holds the value") {
  auto okResult = result<int>::success(42);
  CHECK(okResult.ok());
  CHECK(okResult.value() == 42);
}

TEST_CASE("result failure holds the message") {
  auto failResult = result<int>::failure("Network timeout");
  CHECK_FALSE(failResult.ok());
  CHECK(failResult.errorMessage() == "Network timeout");
}
