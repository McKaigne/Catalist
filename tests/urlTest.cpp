#include <doctest/doctest.h>

#include "catalist/core/url.hpp"

TEST_CASE("url encode keeps unreserved characters only") {
  CHECK(catalist::url::encode("aZ09-_.~") == "aZ09-_.~");
  CHECK(catalist::url::encode("a b&c=d/é") == "a%20b%26c%3dd%2f%c3%a9");
}
