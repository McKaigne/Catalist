#include "system.hpp"

#include <doctest/doctest.h>
#include <stdlib.h>

#include <filesystem>
#include <string>

#include "support/test_support.hpp"

using catalist::test::TempDirFixture;

TEST_CASE_FIXTURE(TempDirFixture, "readFile reads a file, and reports one that is missing") {
	catalist::test::WriteText(TempDir() / "file.txt", "content");
	CHECK(catalist::ReadFile(TempDir() / "file.txt") == "content");
	CHECK_FALSE(catalist::ReadFile(TempDir() / "missing.txt").has_value());
	std::filesystem::create_directories(TempDir() / "taken");
	CHECK_FALSE(catalist::ReadFile(TempDir() / "taken").has_value());
}

TEST_CASE("runProgram passes input through and returns the output") {
	CHECK(catalist::RunProgram({"cat"}, "hello") == "hello");
	// More than a pipe holds, so writing and reading must overlap.
	std::string big(1 << 20, 'x');
	CHECK(catalist::RunProgram({"cat"}, big) == big);
}

TEST_CASE("runProgram hands arguments over verbatim, without a shell") {
	CHECK(catalist::RunProgram({"printf", "%s", "$(echo hi); 'x' `y`"}) == "$(echo hi); 'x' `y`");
}

TEST_CASE("runProgram reports programs that fail or do not exist") {
	CHECK_FALSE(catalist::RunProgram({"sh", "-c", "exit 3"}).has_value());
	CHECK_FALSE(catalist::RunProgram({"catalist-no-such-program"}).has_value());
}

TEST_CASE("localTime follows the computer's calendar and clock") {
	auto date = catalist::IsoDate::FromLocalTm(catalist::LocalTime());
	CHECK(catalist::IsoDate::Parse(date.ToString()) == date);
	auto stamp = catalist::StampOf(catalist::LocalTime());
	REQUIRE(stamp.size() == 19);
	CHECK(catalist::IsoDate::Parse(stamp).has_value());
}

TEST_CASE("runProgram survives a program that exits without reading its input") {
	std::string big(1 << 20, 'x');
	CHECK(catalist::RunProgram({"sh", "-c", "exit 0"}, big) == "");
	CHECK_FALSE(catalist::RunProgram({"catalist-no-such-program"}, big).has_value());
}

TEST_CASE_FIXTURE(TempDirFixture, "httpFetch asks curl to keep the POST across redirects") {
	auto stub = TempDir() / "curl";
	catalist::test::WriteText(stub, "#!/bin/sh\nprintf '%s ' \"$@\"\nprintf '\\n200'\n");
	std::filesystem::permissions(stub, std::filesystem::perms::owner_all);
	std::string path = std::getenv("PATH");
	::setenv("PATH", (TempDir().string() + ":" + path).c_str(), 1);
	auto reply = catalist::HttpFetch({"https://example.test", {"A: b"}, "body"});
	::setenv("PATH", path.c_str(), 1);
	REQUIRE(reply.has_value());
	CHECK(reply->status == 200);
	CHECK(reply->body.find("-X POST --data-binary @- https://example.test") != std::string::npos);
}

TEST_CASE("expandHome replaces only a leading ~ path") {
	std::string home = std::getenv("HOME");
	CHECK(home.find("catalist-tests-home-") != std::string::npos);
	CHECK(catalist::ExpandHome("~/list.xml") == home + "/list.xml");
	CHECK(catalist::ExpandHome("~") == home);
	CHECK(catalist::ExpandHome("~other/list.xml") == "~other/list.xml");
	CHECK(catalist::ExpandHome("list.xml") == "list.xml");
}
