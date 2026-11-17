#include "system.hpp"

#include <doctest/doctest.h>

#include <filesystem>

#include "support/test_support.hpp"

using catalist::test::TempDirFixture;

TEST_CASE_FIXTURE(TempDirFixture, "readFile reads a file, and reports one that is missing") {
	catalist::test::WriteText(TempDir() / "file.txt", "content");
	CHECK(catalist::ReadFile(TempDir() / "file.txt") == "content");
	CHECK_FALSE(catalist::ReadFile(TempDir() / "missing.txt").has_value());
	// Reading a directory used to throw instead of reporting "no file".
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
	// The test main pins TZ=UTC; the date must be a real one, not a default.
	auto date = catalist::IsoDate::FromLocalTm(catalist::LocalTime());
	CHECK(date.Year() >= 2024);
	CHECK(catalist::IsoDate::Parse(date.ToString()) == date);
	auto stamp = catalist::StampOf(catalist::LocalTime());
	REQUIRE(stamp.size() == 19);
	CHECK(catalist::IsoDate::Parse(stamp).has_value());
}

TEST_CASE_FIXTURE(catalist::test::TempDirFixture,
                  "writePrivateFile leaves a file only its owner reads") {
	auto file = TempDir() / "deep" / "accounts";
	REQUIRE(catalist::WritePrivateFile(file, "secret\n"));
	CHECK(catalist::test::ReadText(file) == "secret\n");
	auto mode = std::filesystem::status(file).permissions();
	CHECK((mode & std::filesystem::perms::all) ==
	      (std::filesystem::perms::owner_read | std::filesystem::perms::owner_write));
	REQUIRE(catalist::WritePrivateFile(file, "again\n"));
	CHECK(catalist::test::ReadText(file) == "again\n");
	CHECK(catalist::RandomText(43).size() == 43);
	CHECK(catalist::RandomText(16) != catalist::RandomText(16));
}
