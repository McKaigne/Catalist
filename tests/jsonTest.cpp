#include <doctest/doctest.h>

#include "catalist/core/json.hpp"

namespace json = catalist::json;

TEST_CASE("json parses nested objects arrays and scalars") {
  auto parsed = json::parse(
      R"({"data": [{"node": {"id": 5, "title": "A\"b"}, "score": 7.5}],
          "ok": true, "none": null, "neg": -2e1})");
  REQUIRE(parsed.ok());
  const auto& doc = parsed.value();
  REQUIRE(doc["data"].items().size() == 1);
  CHECK(doc["data"].items()[0]["node"]["id"].asInteger() == 5);
  CHECK(doc["data"].items()[0]["node"]["title"].asString() == "A\"b");
  CHECK(doc["data"].items()[0]["score"].asNumber() == doctest::Approx(7.5));
  CHECK_FALSE(doc["data"].items()[0]["score"].asInteger().has_value());
  CHECK(doc["ok"].asBool() == true);
  CHECK(doc["none"].isNull());
  CHECK(doc["neg"].asInteger() == -20);
  CHECK(doc["missing"]["deeper"].isNull());
}

TEST_CASE("json decodes unicode escapes including surrogate pairs") {
  auto parsed = json::parse(R"(["éあ😀"])");
  REQUIRE(parsed.ok());
  CHECK(parsed.value().items()[0].asString() == "é\xe3\x81\x82\xf0\x9f\x98\x80");
}

TEST_CASE("json rejects malformed input") {
  CHECK_FALSE(json::parse("").ok());
  CHECK_FALSE(json::parse("{\"a\": }").ok());
  CHECK_FALSE(json::parse("[1, 2").ok());
  CHECK_FALSE(json::parse("\"open").ok());
  CHECK_FALSE(json::parse("{} trailing").ok());
  CHECK_FALSE(json::parse(std::string(100, '[') + std::string(100, ']')).ok());
}
