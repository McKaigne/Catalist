#include <doctest/doctest.h>

#include <filesystem>

#include "support/testSupport.hpp"
#include "system.hpp"

using catalist::test::tempDirFixture;

TEST_CASE_FIXTURE(tempDirFixture, "readFile reads a file, and reports one that is missing") {
	catalist::test::writeText(tempDir() / "file.txt", "content");
	CHECK(catalist::readFile(tempDir() / "file.txt") == "content");
	CHECK_FALSE(catalist::readFile(tempDir() / "missing.txt").has_value());
	// Reading a directory used to throw instead of reporting "no file".
	std::filesystem::create_directories(tempDir() / "taken");
	CHECK_FALSE(catalist::readFile(tempDir() / "taken").has_value());
}

TEST_CASE("runProgram passes input through and returns the output") {
	CHECK(catalist::runProgram({"cat"}, "hello") == "hello");
	// More than a pipe holds, so writing and reading must overlap.
	std::string big(1 << 20, 'x');
	CHECK(catalist::runProgram({"cat"}, big) == big);
}

TEST_CASE("runProgram hands arguments over verbatim, without a shell") {
	CHECK(catalist::runProgram({"printf", "%s", "$(echo hi); 'x' `y`"}) == "$(echo hi); 'x' `y`");
}

TEST_CASE("runProgram reports programs that fail or do not exist") {
	CHECK_FALSE(catalist::runProgram({"sh", "-c", "exit 3"}).has_value());
	CHECK_FALSE(catalist::runProgram({"catalist-no-such-program"}).has_value());
}

TEST_CASE("today follows the computer's calendar") {
	// The test main pins TZ=UTC; the date must be a real one, not a default.
	auto date = catalist::today();
	CHECK(date.year() >= 2024);
	CHECK(catalist::isoDate::parse(date.toString()) == date);
}

TEST_CASE("nowStamp is the date and the time of day") {
	auto stamp = catalist::nowStamp();
	REQUIRE(stamp.size() == 19);
	CHECK(catalist::isoDate::parse(stamp).has_value());
	CHECK(stamp[10] == ' ');
	CHECK(stamp[13] == ':');
	CHECK(stamp[16] == ':');
}

TEST_CASE_FIXTURE(catalist::test::tempDirFixture,
                  "writePrivateFile leaves a file only its owner reads") {
	auto file = tempDir() / "deep" / "accounts";
	REQUIRE(catalist::writePrivateFile(file, "secret\n"));
	CHECK(catalist::test::readText(file) == "secret\n");
	auto mode = std::filesystem::status(file).permissions();
	CHECK((mode & std::filesystem::perms::all) ==
	      (std::filesystem::perms::owner_read | std::filesystem::perms::owner_write));
	REQUIRE(catalist::writePrivateFile(file, "again\n"));
	CHECK(catalist::test::readText(file) == "again\n");
	CHECK(catalist::randomText(43).size() == 43);
	CHECK(catalist::randomText(16) != catalist::randomText(16));
}
