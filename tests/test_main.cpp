#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include <cstdlib>
#include <ctime>
#include <filesystem>

#include "support/test_support.hpp"

int main(int argc, char** argv) {
	auto sandbox_root = catalist::test::MakeUniqueTempDir("catalist-tests-env-");
	auto home_dir = sandbox_root / "home";
	std::filesystem::create_directories(home_dir);

	::setenv("HOME", home_dir.c_str(), 1);
	::setenv("XDG_CONFIG_HOME", (home_dir / ".config").c_str(), 1);
	::setenv("XDG_DATA_HOME", (home_dir / ".local" / "share").c_str(), 1);
	::setenv("NO_COLOR", "1", 1);
	::setenv("TZ", "UTC", 1);
	::tzset();

	doctest::Context context(argc, argv);
	int exit_code = context.run();

	std::error_code ignored;
	std::filesystem::remove_all(sandbox_root, ignored);
	return exit_code;
}
