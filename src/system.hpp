#ifndef CATALIST_SYSTEM_HPP_
#define CATALIST_SYSTEM_HPP_

#include <cstddef>
#include <filesystem>
#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

#include "core/date.hpp"
#include "web/http.hpp"

// Everything Catalist does to the world outside the program: reading files,
// other programs, the network, the clock and the terminal. Nothing else in the
// code touches them, so the rest can be tested with temporary directories
// and plain functions.
namespace catalist {

// Absent when the file does not exist or cannot be read.
[[nodiscard]] std::optional<std::string> readFile(const std::filesystem::path& file);

// Runs a program (looked up in PATH, no shell involved) with `input` on its
// standard input, and returns what it wrote to standard output; nothing when
// it could not be started or did not exit with 0. Standard error is dropped.
[[nodiscard]] std::optional<std::string> runProgram(const std::vector<std::string>& arguments,
                                                    const std::string& input = "");

// Sends the request through the curl program: the body of the reply, or
// nothing when the request failed for any reason (no connection, a timeout,
// an HTTP error status).
[[nodiscard]] std::optional<std::string> httpFetch(const web::request& sent);

// The date on this computer's calendar.
[[nodiscard]] isoDate today();

// The local date and time, "YYYY-MM-DD HH:MM:SS": when a change was made,
// for the undo history.
[[nodiscard]] std::string nowStamp();

// Whether standard input and output are both a terminal (someone can be
// asked a question), and the width of standard output's terminal in columns
// (0 when it is not one).
[[nodiscard]] bool isInteractive();
[[nodiscard]] int terminalColumns();

// Writes the file so that only its owner can read it (mode 600), making the
// directories it is in; false when that failed.
[[nodiscard]] bool writePrivateFile(const std::filesystem::path& file, const std::string& text);

// A line of `in`, not shown as it is typed when standard input is a
// terminal (a password); nothing at the end of the input.
[[nodiscard]] std::optional<std::string> readSecretLine(std::istream& in);

// Letters, digits and -._~ from /dev/urandom: what OAuth's PKCE wants.
[[nodiscard]] std::string randomText(size_t length);

// Seconds since 1970.
[[nodiscard]] long long unixNow();

}  // namespace catalist

#endif  // CATALIST_SYSTEM_HPP_
