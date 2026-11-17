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
[[nodiscard]] std::optional<std::string> ReadFile(const std::filesystem::path& file);

// Runs a program (looked up in PATH, no shell involved) with `input` on its
// standard input, and returns what it wrote to standard output; nothing when
// it could not be started or did not exit with 0. Standard error is dropped.
[[nodiscard]] std::optional<std::string> RunProgram(const std::vector<std::string>& arguments,
                                                    const std::string& input = "");

// Sends the request through the curl program: the reply, whatever its
// status, or nothing when there was none (no connection, a timeout).
[[nodiscard]] std::optional<web::Reply> HttpFetch(const web::Request& sent);

// The date and time on this computer's calendar and clock.
[[nodiscard]] std::tm LocalTime();

// Whether standard input and output are both a terminal (someone can be
// asked a question), and the width of standard output's terminal in columns
// (0 when it is not one).
[[nodiscard]] bool IsInteractive();
[[nodiscard]] int TerminalColumns();

// Writes the file so that only its owner can read it (mode 600), making the
// directories it is in; false when that failed.
[[nodiscard]] bool WritePrivateFile(const std::filesystem::path& file, const std::string& text);

// A line of `in`, not shown as it is typed when standard input is a
// terminal (a password); nothing at the end of the input.
[[nodiscard]] std::optional<std::string> ReadSecretLine(std::istream& in);

// Letters, digits and -._~ from /dev/urandom: what OAuth's PKCE wants.
[[nodiscard]] std::string RandomText(size_t length);

// Seconds since 1970.
[[nodiscard]] long long UnixNow();

}  // namespace catalist

#endif  // CATALIST_SYSTEM_HPP_
