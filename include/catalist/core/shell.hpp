#ifndef CATALIST_CORE_SHELL_HPP_
#define CATALIST_CORE_SHELL_HPP_

#include <string>
#include <string_view>
#include <vector>

namespace catalist::shell {

// POSIX single-quote escaping: the result is always one shell word, whatever
// the input contains.
[[nodiscard]] std::string escapeShellArg(std::string_view arg);

[[nodiscard]] std::string joinEscaped(const std::vector<std::string>& argv);

}  // namespace catalist::shell

#endif  // CATALIST_CORE_SHELL_HPP_
