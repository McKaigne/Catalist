#include "web/json.hpp"

#include <doctest/doctest.h>

namespace json = catalist::json;

TEST_CASE("json parses nested objects arrays and scalars") {
	auto parsed = json::Parse(
	    R"({"data": [{"node": {"id": 5, "title": "A\"b"}, "score": 7.5}],
          "ok": true, "none": null, "neg": -2e1})");
	REQUIRE(parsed.has_value());
	const auto& doc = parsed.value();
	REQUIRE(doc["data"].Items().size() == 1);
	CHECK(doc["data"].Items()[0]["node"]["id"].AsInteger() == 5);
	CHECK(doc["data"].Items()[0]["node"]["title"].AsString() == "A\"b");
	CHECK(doc["data"].Items()[0]["score"].AsNumber() == doctest::Approx(7.5));
	CHECK_FALSE(doc["data"].Items()[0]["score"].AsInteger().has_value());
	CHECK_FALSE(doc["ok"].IsNull());
	CHECK_FALSE(doc["ok"].AsNumber().has_value());
	CHECK(doc["none"].IsNull());
	CHECK(doc["neg"].AsInteger() == -20);
	CHECK(doc["missing"]["deeper"].IsNull());
	// A count is a whole number above 0; anything else counts as none.
	CHECK(doc["data"].Items()[0]["node"]["id"].AsCount() == 5);
	CHECK(doc["data"].Items()[0]["score"].AsCount() == 0);
	CHECK(doc["neg"].AsCount() == 0);
	CHECK(doc["none"].AsCount() == 0);
}

TEST_CASE("json decodes unicode escapes including surrogate pairs") {
	auto parsed = json::Parse(R"(["éあ😀"])");
	REQUIRE(parsed.has_value());
	CHECK(parsed.value().Items()[0].AsString() == "é\xe3\x81\x82\xf0\x9f\x98\x80");
}

TEST_CASE("json rejects malformed input") {
	CHECK_FALSE(json::Parse("").has_value());
	CHECK_FALSE(json::Parse("{\"a\": }").has_value());
	CHECK_FALSE(json::Parse("[1, 2").has_value());
	CHECK_FALSE(json::Parse("\"open").has_value());
	CHECK_FALSE(json::Parse("{} trailing").has_value());
	CHECK_FALSE(json::Parse(std::string(100, '[') + std::string(100, ']')).has_value());
}
