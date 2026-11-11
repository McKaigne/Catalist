#ifndef CATALIST_SYSTEM_HPP_
#define CATALIST_SYSTEM_HPP_

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "date.hpp"

// Everything Catalist does to the world outside the program: files, other
// programs, the network, the clock and the terminal. Nothing else in the
// code touches them, so the rest can be tested with temporary directories
// and plain functions.
namespace catalist {

// Absent when the file does not exist or cannot be read.
[[nodiscard]] std::optional<std::string> readFile(
    const std::filesystem::path& file);

// Creates the parent directory if needed, writes a sibling temp file and
// renames it over the target, so a crash never leaves a half-written file.
[[nodiscard]] bool writeFileAtomic(const std::filesystem::path& file,
                                   const std::string& content);

// Runs a program (looked up in PATH, no shell involved) with `input` on its
// standard input, and returns what it wrote to standard output; nothing when
// it could not be started or did not exit with 0. Standard error is dropped.
[[nodiscard]] std::optional<std::string> runProgram(
    const std::vector<std::string>& arguments, const std::string& input = "");

// Whether a program of that name is in PATH.
[[nodiscard]] bool hasProgram(const std::string& name);

// The body of a GET through the curl program, or nothing when the request
// failed for any reason: no connection, a timeout or an HTTP error status.
[[nodiscard]] std::optional<std::string> httpGet(const std::string& url);

// The date on this computer's calendar.
[[nodiscard]] isoDate today();

// The size of one terminal cell in pixels, when the terminal reports it.
struct cellSize {
  int width = 0;
  int height = 0;
};
[[nodiscard]] std::optional<cellSize> terminalCellSize();

}  // namespace catalist

#endif  // CATALIST_SYSTEM_HPP_
