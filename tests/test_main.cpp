#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>
#include <stdlib.h>
#include <time.h>

#include <filesystem>
#include <system_error>

#include "support/test_support.hpp"

int main(int argc, char** argv) {
	auto home = catalist::test::MakeUniqueTempDir("catalist-tests-home-");
	::setenv("HOME", home.c_str(), 1);
	::setenv("NO_COLOR", "1", 1);
	::setenv("TZ", "UTC", 1);
	::unsetenv("CATALIST_CSV");
	::tzset();

	doctest::Context context(argc, argv);
	int exit_code = context.run();

	std::error_code ignored;
	std::filesystem::remove_all(home, ignored);
	return exit_code;
}
