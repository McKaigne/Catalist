#ifndef CATALIST_SYSTEM_HPP_
#define CATALIST_SYSTEM_HPP_

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "date.hpp"

// Everything Catalist does to the world outside the program: files, the
// network and the clock. Nothing else in the code touches them, so the rest
// can be tested with temporary directories and plain functions.
namespace catalist {

// Absent when the file does not exist or cannot be read.
[[nodiscard]] std::optional<std::string> readFile(
    const std::filesystem::path& file);

// Creates the parent directory if needed, writes a sibling temp file and
// renames it over the target, so a crash never leaves a half-written file.
[[nodiscard]] bool writeFileAtomic(const std::filesystem::path& file,
                                   const std::string& content);

// The body of a GET through the curl program, or nothing when the request
// failed for any reason: no connection, a timeout or an HTTP error status.
[[nodiscard]] std::optional<std::string> httpGet(const std::string& url);

// The date on this computer's calendar.
[[nodiscard]] isoDate today();

// POSIX single-quote escaping: the result is always one shell word, whatever
// the input contains.
[[nodiscard]] std::string shellQuote(std::string_view text);

}  // namespace catalist

#endif  // CATALIST_SYSTEM_HPP_
