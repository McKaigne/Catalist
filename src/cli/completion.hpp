#ifndef CATALIST_CLI_COMPLETION_HPP_
#define CATALIST_CLI_COMPLETION_HPP_

#include <string>
#include <vector>

#include "cli/cli.hpp"

// What the shells and the manual page are told about the command line. All
// of it is written from the command table, so none of it can fall behind
// the program. Pure: a table in, text out.
//
// The scripts know the commands and their options by heart. What only the
// lists can tell (the refs and the titles) they ask the program for as the
// person types, with `catalist __complete refs|titles|statuses|fields`.
namespace catalist::completion {

// The completion function for zsh: the file `_catalist` of a directory in
// $fpath.
[[nodiscard]] std::string zsh(const std::vector<command>& table);

// The completions for fish: the file `catalist.fish`.
[[nodiscard]] std::string fish(const std::vector<command>& table);

// The COMMANDS section of the manual page, in roff: each command with its
// usage line, what it does and its options.
[[nodiscard]] std::string manual(const std::vector<command>& table);

}  // namespace catalist::completion

#endif  // CATALIST_CLI_COMPLETION_HPP_
