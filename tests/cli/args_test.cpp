#include "cli/args.hpp"

#include <doctest/doctest.h>

#include <sstream>

namespace {

using catalist::args::Option;
using catalist::args::Parse;

const std::vector<Option> kOptions = {{'a', "", "", "anime"},          {'m', "", "", "manga"},
                                      {'n', "", "N", "a count"},       {'f', "force", "", "force"},
                                      {0, "sort", "KEY", "the order"}, {0, "json", "", "JSON"}};

using Words = std::vector<std::string>;

}  // namespace

TEST_CASE("args reads flags, one at a time or grouped") {
	auto line = Parse({"-a", "-m"}, kOptions);
	REQUIRE(line.has_value());
	CHECK(line.value().Has("a"));
	CHECK(line.value().Has("m"));
	CHECK_FALSE(line.value().Has("force"));
	CHECK(line.value().operands.empty());

	line = Parse({"-amf"}, kOptions);
	REQUIRE(line.has_value());
	CHECK(line.value().Has("a"));
	CHECK(line.value().Has("m"));
	// An Option with both forms is known by its long name.
	CHECK(line.value().Has("force"));
	CHECK_FALSE(line.value().Has("f"));
}

TEST_CASE("args reads a short option's value attached or as the next argument") {
	for (const auto& given : {Words{"-n", "3"}, Words{"-n3"}, Words{"-an3"}, Words{"-an", "3"}}) {
		auto line = Parse(given, kOptions);
		REQUIRE(line.has_value());
		CHECK(line.value().Get("n") == "3");
		CHECK(line.value().operands.empty());
	}
	// The value is whatever follows, even when it looks like an Option.
	auto line = Parse({"-n", "-a"}, kOptions);
	REQUIRE(line.has_value());
	CHECK(line.value().Get("n") == "-a");
	CHECK_FALSE(line.value().Has("a"));
}

TEST_CASE("args reads long options, with = or with the next argument") {
	auto line = Parse({"--json", "--sort=title", "--force"}, kOptions);
	REQUIRE(line.has_value());
	CHECK(line.value().Has("json"));
	CHECK(line.value().Get("sort") == "title");
	CHECK(line.value().Has("force"));

	line = Parse({"--sort", "score", "frieren"}, kOptions);
	REQUIRE(line.has_value());
	CHECK(line.value().Get("sort") == "score");
	CHECK(line.value().operands == Words{"frieren"});

	// An empty value is a value; the last of a repeated Option counts.
	line = Parse({"--sort=", "--sort=a=b"}, kOptions);
	REQUIRE(line.has_value());
	CHECK(line.value().Has("sort"));
	CHECK(line.value().Get("sort") == "a=b");
	// An Option that was not given reads as empty.
	CHECK(line.value().Get("n").empty());
}

TEST_CASE("args ends the options at the first operand") {
	auto line = Parse({"-a", "frieren", "-m", "--json"}, kOptions);
	REQUIRE(line.has_value());
	CHECK(line.value().Has("a"));
	CHECK_FALSE(line.value().Has("m"));
	CHECK_FALSE(line.value().Has("json"));
	CHECK(line.value().operands == Words{"frieren", "-m", "--json"});
}

TEST_CASE("args ends the options at --, and takes a lone - as an operand") {
	auto line = Parse({"-a", "--", "-m", "--json"}, kOptions);
	REQUIRE(line.has_value());
	CHECK(line.value().Has("a"));
	CHECK(line.value().operands == Words{"-m", "--json"});

	// Only the first -- is taken away.
	line = Parse({"--", "--", "x"}, kOptions);
	REQUIRE(line.has_value());
	CHECK(line.value().operands == Words{"--", "x"});

	line = Parse({"-a", "-", "-m"}, kOptions);
	REQUIRE(line.has_value());
	CHECK(line.value().operands == Words{"-", "-m"});

	line = Parse({}, kOptions);
	REQUIRE(line.has_value());
	CHECK(line.value().options.empty());
	CHECK(line.value().operands.empty());
	line = Parse({""}, kOptions);
	REQUIRE(line.has_value());
	CHECK(line.value().operands == Words{""});
}

TEST_CASE("args says what is wrong with an option") {
	CHECK(Parse({"-x"}, kOptions).error() == "unknown option '-x'");
	CHECK(Parse({"-ax"}, kOptions).error() == "unknown option '-x'");
	CHECK(Parse({"--frobnicate"}, kOptions).error() == "unknown option '--frobnicate'");
	CHECK(Parse({"--jsno"}, kOptions).error() ==
	      "unknown option '--jsno' (did you mean '--json'?)");
	CHECK(Parse({"--forse=1"}, kOptions).error() ==
	      "unknown option '--forse' (did you mean '--force'?)");
	CHECK(Parse({"-n"}, kOptions).error() == "option '-n' needs a value (N)");
	CHECK(Parse({"-an"}, kOptions).error() == "option '-n' needs a value (N)");
	CHECK(Parse({"--sort"}, kOptions).error() == "option '--sort' needs a value (KEY)");
	CHECK(Parse({"--json=yes"}, kOptions).error() == "option '--json' takes no value");
	// A short Option has no long form unless it is given one.
	CHECK_FALSE(Parse({"--a"}, kOptions).has_value());
	CHECK(Parse({"--"}, kOptions).has_value());
}

TEST_CASE("args spells an option by its long name, or its letter") {
	CHECK(catalist::args::Spelled(kOptions[0]) == "-a");
	CHECK(catalist::args::Spelled(kOptions[3]) == "--force");
	CHECK(catalist::args::Spelled(kOptions[4]) == "--sort");
}

TEST_CASE("args reads operands from the input where there is a -") {
	std::istringstream in("a:1\n\n  \nSousou no Frieren\r\nm:2");
	CHECK(catalist::args::WithLinesFor({"first", "-", "last"}, in) ==
	      Words{"first", "a:1", "Sousou no Frieren", "m:2", "last"});
	std::istringstream untouched("a:1\n");
	CHECK(catalist::args::WithLinesFor({"first", "last"}, untouched) == Words{"first", "last"});
	std::string left;
	CHECK(std::getline(untouched, left));
	CHECK(left == "a:1");
	std::istringstream empty("");
	CHECK(catalist::args::WithLinesFor({"-"}, empty).empty());
}

TEST_CASE("args suggests the closest known word for a typo") {
	const Words known = {"ls", "show", "set", "search", "stats"};
	CHECK(catalist::args::ClosestTo("lst", known) == "ls");
	CHECK(catalist::args::ClosestTo("serach", known) == "search");
	CHECK(catalist::args::ClosestTo("shwo", known) == "show");
	CHECK_FALSE(catalist::args::ClosestTo("sync", known).has_value());
	// Not when every letter would have to change.
	CHECK_FALSE(catalist::args::ClosestTo("x", known).has_value());
}

TEST_CASE("args reads a count of digits only") {
	CHECK(catalist::args::ParseCount("0") == 0);
	CHECK(catalist::args::ParseCount("28") == 28);
	CHECK(catalist::args::ParseCount("999999") == 999999);
	for (const char* bad : {"", "-1", "+1", "1.5", "3x", " 3", "1234567"})
		CHECK_FALSE(catalist::args::ParseCount(bad).has_value());
}

TEST_CASE("args reads a site id beyond six digits") {
	CHECK(catalist::args::ParseId("1") == 1);
	CHECK(catalist::args::ParseId("1234567") == 1234567);
	for (const char* bad : {"", "0", "-1", "+1", "1x", "99999999999"})
		CHECK_FALSE(catalist::args::ParseId(bad).has_value());
}
