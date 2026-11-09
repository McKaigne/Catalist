#include <doctest/doctest.h>

#include "catalist/core/url.hpp"

TEST_CASE("url encode keeps unreserved characters only") {
  CHECK(catalist::url::encode("aZ09-_.~") == "aZ09-_.~");
  CHECK(catalist::url::encode("a b&c=d/é") == "a%20b%26c%3dd%2f%c3%a9");
}

TEST_CASE("url decode reverses encode and treats plus as space") {
  CHECK(catalist::url::decode("a%20b%26c%3Dd") == "a b&c=d");
  CHECK(catalist::url::decode("a+b") == "a b");
  CHECK(catalist::url::decode(catalist::url::encode("x\"; $(id)")) ==
        "x\"; $(id)");
}

TEST_CASE("url decode rejects truncated or non-hex escapes") {
  CHECK_FALSE(catalist::url::decode("%2").has_value());
  CHECK_FALSE(catalist::url::decode("%g0").has_value());
}
