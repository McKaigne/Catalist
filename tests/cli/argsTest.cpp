#include <doctest/doctest.h>

#include <sstream>

#include "cli/args.hpp"

namespace {

using catalist::args::option;
using catalist::args::parse;

const std::vector<option> kOptions = {{'a', "", "", "anime"},          {'m', "", "", "manga"},
                                      {'n', "", "N", "a count"},       {'f', "force", "", "force"},
                                      {0, "sort", "KEY", "the order"}, {0, "json", "", "JSON"}};

using words = std::vector<std::string>;

}  // namespace

TEST_CASE("args reads flags, one at a time or grouped") {
	auto line = parse({"-a", "-m"}, kOptions);
	REQUIRE(line.ok());
	CHECK(line.value().has("a"));
	CHECK(line.value().has("m"));
	CHECK_FALSE(line.value().has("force"));
	CHECK(line.value().operands.empty());

	line = parse({"-amf"}, kOptions);
	REQUIRE(line.ok());
	CHECK(line.value().has("a"));
	CHECK(line.value().has("m"));
	// An option with both forms is known by its long name.
	CHECK(line.value().has("force"));
	CHECK_FALSE(line.value().has("f"));
}

TEST_CASE("args reads a short option's value attached or as the next argument") {
	for (const auto& given : {words{"-n", "3"}, words{"-n3"}, words{"-an3"}, words{"-an", "3"}}) {
		auto line = parse(given, kOptions);
		REQUIRE(line.ok());
		CHECK(line.value().get("n") == "3");
		CHECK(line.value().operands.empty());
	}
	// The value is whatever follows, even when it looks like an option.
	auto line = parse({"-n", "-a"}, kOptions);
	REQUIRE(line.ok());
	CHECK(line.value().get("n") == "-a");
	CHECK_FALSE(line.value().has("a"));
}

TEST_CASE("args reads long options, with = or with the next argument") {
	auto line = parse({"--json", "--sort=title", "--force"}, kOptions);
	REQUIRE(line.ok());
	CHECK(line.value().has("json"));
	CHECK(line.value().get("sort") == "title");
	CHECK(line.value().has("force"));

	line = parse({"--sort", "score", "frieren"}, kOptions);
	REQUIRE(line.ok());
	CHECK(line.value().get("sort") == "score");
	CHECK(line.value().operands == words{"frieren"});

	// An empty value is a value; the last of a repeated option counts.
	line = parse({"--sort=", "--sort=a=b"}, kOptions);
	REQUIRE(line.ok());
	CHECK(line.value().has("sort"));
	CHECK(line.value().get("sort") == "a=b");
	// An option that was not given reads as empty.
	CHECK(line.value().get("n").empty());
}

TEST_CASE("args ends the options at the first operand") {
	auto line = parse({"-a", "frieren", "-m", "--json"}, kOptions);
	REQUIRE(line.ok());
	CHECK(line.value().has("a"));
	CHECK_FALSE(line.value().has("m"));
	CHECK_FALSE(line.value().has("json"));
	CHECK(line.value().operands == words{"frieren", "-m", "--json"});
}

TEST_CASE("args ends the options at --, and takes a lone - as an operand") {
	auto line = parse({"-a", "--", "-m", "--json"}, kOptions);
	REQUIRE(line.ok());
	CHECK(line.value().has("a"));
	CHECK(line.value().operands == words{"-m", "--json"});

	// Only the first -- is taken away.
	line = parse({"--", "--", "x"}, kOptions);
	REQUIRE(line.ok());
	CHECK(line.value().operands == words{"--", "x"});

	line = parse({"-a", "-", "-m"}, kOptions);
	REQUIRE(line.ok());
	CHECK(line.value().operands == words{"-", "-m"});

	line = parse({}, kOptions);
	REQUIRE(line.ok());
	CHECK(line.value().options.empty());
	CHECK(line.value().operands.empty());
	line = parse({""}, kOptions);
	REQUIRE(line.ok());
	CHECK(line.value().operands == words{""});
}

TEST_CASE("args says what is wrong with an option") {
	CHECK(parse({"-x"}, kOptions).errorMessage() == "unknown option '-x'");
	CHECK(parse({"-ax"}, kOptions).errorMessage() == "unknown option '-x'");
	CHECK(parse({"--frobnicate"}, kOptions).errorMessage() == "unknown option '--frobnicate'");
	CHECK(parse({"--jsno"}, kOptions).errorMessage() ==
	      "unknown option '--jsno' (did you mean '--json'?)");
	CHECK(parse({"--forse=1"}, kOptions).errorMessage() ==
	      "unknown option '--forse' (did you mean '--force'?)");
	CHECK(parse({"-n"}, kOptions).errorMessage() == "option '-n' needs a value (N)");
	CHECK(parse({"-an"}, kOptions).errorMessage() == "option '-n' needs a value (N)");
	CHECK(parse({"--sort"}, kOptions).errorMessage() == "option '--sort' needs a value (KEY)");
	CHECK(parse({"--json=yes"}, kOptions).errorMessage() == "option '--json' takes no value");
	// A short option has no long form unless it is given one.
	CHECK_FALSE(parse({"--a"}, kOptions).ok());
	CHECK(parse({"--"}, kOptions).ok());
}

TEST_CASE("args spells an option by its long name, or its letter") {
	CHECK(catalist::args::spelled(kOptions[0]) == "-a");
	CHECK(catalist::args::spelled(kOptions[3]) == "--force");
	CHECK(catalist::args::spelled(kOptions[4]) == "--sort");
}

TEST_CASE("args reads operands from the input where there is a -") {
	std::istringstream in("a:1\n\n  \nSousou no Frieren\r\nm:2");
	CHECK(catalist::args::withLinesFor({"first", "-", "last"}, in) ==
	      words{"first", "a:1", "Sousou no Frieren", "m:2", "last"});
	std::istringstream untouched("a:1\n");
	CHECK(catalist::args::withLinesFor({"first", "last"}, untouched) == words{"first", "last"});
	std::string left;
	CHECK(std::getline(untouched, left));
	CHECK(left == "a:1");
	std::istringstream empty("");
	CHECK(catalist::args::withLinesFor({"-"}, empty).empty());
}

TEST_CASE("args suggests the closest known word for a typo") {
	const words known = {"ls", "show", "set", "search", "stats"};
	CHECK(catalist::args::closestTo("lst", known) == "ls");
	CHECK(catalist::args::closestTo("serach", known) == "search");
	CHECK(catalist::args::closestTo("shwo", known) == "show");
	CHECK_FALSE(catalist::args::closestTo("sync", known).has_value());
	// Not when every letter would have to change.
	CHECK_FALSE(catalist::args::closestTo("x", known).has_value());
}

TEST_CASE("args reads a count of digits only") {
	CHECK(catalist::args::parseCount("0") == 0);
	CHECK(catalist::args::parseCount("28") == 28);
	CHECK(catalist::args::parseCount("999999") == 999999);
	for (const char* bad : {"", "-1", "+1", "1.5", "3x", " 3", "1234567"})
		CHECK_FALSE(catalist::args::parseCount(bad).has_value());
}
