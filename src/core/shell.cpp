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

std::string joinEscaped(const std::vector<std::string>& argv) {
  std::string command;
  for (const auto& arg : argv) {
    if (!command.empty())
      command += ' ';
    command += escapeShellArg(arg);
  }
  return command;
}

}  // namespace catalist::shell
