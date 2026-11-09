#include <doctest/doctest.h>

#include "catalist/core/jsonLite.hpp"

using namespace catalist::jsonLite;

TEST_CASE("extractJsonString reads compact and spaced keys") {
  CHECK(extractJsonString(R"({"title":"Frieren"})", "title") == "Frieren");
  CHECK(extractJsonString(R"({"title": "Frieren"})", "title") == "Frieren");
  CHECK(extractJsonString(R"({"title":""})", "title") == "");
}

TEST_CASE("extractJsonString stops at the first unescaped quote") {
  CHECK(extractJsonString(R"({"t":"say \"hi\"","x":1})", "t") ==
        R"(say \"hi\")");
}

TEST_CASE("extractJsonString reports a missing key as absent") {
  CHECK_FALSE(extractJsonString(R"({"other":"x"})", "title").has_value());
  CHECK_FALSE(extractJsonString(R"({"title":5})", "title").has_value());
}

TEST_CASE("extractJsonNumber reads bare quoted and decimal numbers") {
  CHECK(extractJsonNumber(R"({"n":28})", "n") == 28.0);
  CHECK(extractJsonNumber(R"({"n": 8.94})", "n") == doctest::Approx(8.94));
  CHECK(extractJsonNumber(R"({"n":"89.4"})", "n") == doctest::Approx(89.4));
}

TEST_CASE("extractJsonNumber treats null and missing as absent") {
  CHECK_FALSE(extractJsonNumber(R"({"n":null})", "n").has_value());
  CHECK_FALSE(extractJsonNumber(R"({"m":1})", "n").has_value());
}

TEST_CASE("unescapeJsonString decodes the supported escapes") {
  CHECK(unescapeJsonString(R"(a\"b\\c\/d\ne\tf)") == "a\"b\\c/d\ne\tf");
  CHECK(unescapeJsonString(R"(keep \x and trailing \)") ==
        R"(keep \x and trailing \)");
}

TEST_CASE("escapeJsonString escapes quotes backslashes and control characters") {
  CHECK(escapeJsonString("a\"b\\c\n\x01") == "a\\\"b\\\\c\\n\\u0001");
}

TEST_CASE("splitArrayObjects returns each top level object in the array") {
  auto objects = splitArrayObjects(
      R"({"data":[{"id":1,"t":{"x":"}"}},{"id":2}],"meta":{"id":3}})", "data");
  REQUIRE(objects.size() == 2);
  CHECK(objects[0] == R"({"id":1,"t":{"x":"}"}})");
  CHECK(objects[1] == R"({"id":2})");
}

TEST_CASE("splitArrayObjects handles a missing key and an empty array") {
  CHECK(splitArrayObjects(R"({"other":[{"id":1}]})", "data").empty());
  CHECK(splitArrayObjects(R"({"data":[]})", "data").empty());
}
