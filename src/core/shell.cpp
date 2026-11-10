#include "catalist/core/shell.hpp"

namespace catalist::shell {

std::string escapeShellArg(std::string_view arg) {
  std::string quoted = "'";
  for (char c : arg) {
    if (c == '\'')
      quoted += "'\\''";
    else
      quoted += c;
  }
  quoted += "'";
  return quoted;
}

}  // namespace catalist::shell
