#include <doctest/doctest.h>

#include "catalist/core/shell.hpp"

using catalist::shell::escapeShellArg;

TEST_CASE("escapeShellArg wraps plain text in single quotes") {
  CHECK(escapeShellArg("hello world") == "'hello world'");
  CHECK(escapeShellArg("") == "''");
}

TEST_CASE("escapeShellArg neutralises quotes and substitutions") {
  CHECK(escapeShellArg("it's") == "'it'\\''s'");
  CHECK(escapeShellArg("$(rm -rf ~); `x` \"y\"") ==
        "'$(rm -rf ~); `x` \"y\"'");
}
