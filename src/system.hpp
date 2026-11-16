#ifndef CATALIST_SYSTEM_HPP_
#define CATALIST_SYSTEM_HPP_

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "core/date.hpp"

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

// The body of the reply to a POST of `json` as application/json, through
// the curl program (AniList's API takes only POST), or nothing when the
// request failed for any reason: no connection, a timeout or an HTTP error
// status.
[[nodiscard]] std::optional<std::string> httpPost(const std::string& url, const std::string& json);

// The date on this computer's calendar.
[[nodiscard]] isoDate today();

// The local date and time, "YYYY-MM-DD HH:MM:SS": when a change was made,
// for the undo history.
[[nodiscard]] std::string nowStamp();

// A label no other run of commands has: "<seconds since 1970>-<process
// id>", for CATALIST_TX.
[[nodiscard]] std::string newTxLabel();

// Whether standard input and output are both a terminal (someone can be
// asked a question), and the width of standard output's terminal in columns
// (0 when it is not one).
[[nodiscard]] bool isInteractive();
[[nodiscard]] int terminalColumns();

}  // namespace catalist

#endif  // CATALIST_SYSTEM_HPP_
