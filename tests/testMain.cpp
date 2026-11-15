#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include <cstdlib>
#include <ctime>
#include <filesystem>

#include "support/testSupport.hpp"

// The data directory, HOME and the XDG directories are redirected before any
// test code runs, so no test can touch the developer's real lists.
int main(int argc, char** argv) {
	auto sandboxRoot = catalist::test::makeUniqueTempDir("catalist-tests-env-");
	auto dataDir = sandboxRoot / "data";
	auto homeDir = sandboxRoot / "home";
	std::filesystem::create_directories(dataDir);
	std::filesystem::create_directories(homeDir);

	::setenv("CATALIST_DATA_DIR", dataDir.c_str(), 1);
	::setenv("HOME", homeDir.c_str(), 1);
	::setenv("XDG_CONFIG_HOME", (homeDir / ".config").c_str(), 1);
	::setenv("XDG_DATA_HOME", (homeDir / ".local" / "share").c_str(), 1);
	::setenv("NO_COLOR", "1", 1);
	::setenv("TZ", "UTC", 1);
	::tzset();

	doctest::Context context(argc, argv);
	int exitCode = context.run();

	std::error_code ignored;
	std::filesystem::remove_all(sandboxRoot, ignored);
	return exitCode;
}
