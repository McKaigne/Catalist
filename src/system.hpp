#ifndef CATALIST_SYSTEM_HPP_
#define CATALIST_SYSTEM_HPP_

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "core/date.hpp"
#include "web/http.hpp"

namespace catalist {

[[nodiscard]] std::optional<std::string> ReadFile(const std::filesystem::path& file);
[[nodiscard]] bool WriteFile(const std::filesystem::path& file, const std::string& text);
[[nodiscard]] std::string ExpandHome(const std::string& path);

[[nodiscard]] std::optional<std::string> RunProgram(const std::vector<std::string>& arguments,
                                                    const std::string& input = "");

[[nodiscard]] std::optional<web::Reply> HttpFetch(const web::Request& sent);

[[nodiscard]] std::tm LocalTime();

[[nodiscard]] bool IsInteractive();
[[nodiscard]] int TerminalColumns();

}  // namespace catalist

#endif  // CATALIST_SYSTEM_HPP_
