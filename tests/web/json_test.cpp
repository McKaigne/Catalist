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
	CHECK_FALSE(doc["ok"].AsNumber().has_value());
	CHECK(doc["neg"].AsInteger() == -20);
	CHECK_FALSE(doc["missing"]["deeper"].AsString().has_value());
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

TEST_CASE("json rejects out-of-range numbers in AsInteger and AsCount") {
	auto huge = json::Parse("[1e30, 1e15, -1e30, 9223372036854775808]");
	REQUIRE(huge.has_value());
	const auto& items = huge.value().Items();
	CHECK_FALSE(items[0].AsInteger().has_value());
	CHECK(items[0].AsCount() == 0);
	CHECK(items[1].AsInteger() == 1000000000000000LL);
	CHECK(items[1].AsCount() == 0);
	CHECK_FALSE(items[2].AsInteger().has_value());
	CHECK_FALSE(items[3].AsInteger().has_value());
}
