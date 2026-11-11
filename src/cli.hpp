#ifndef CATALIST_CLI_HPP_
#define CATALIST_CLI_HPP_

#include <iosfwd>
#include <string>
#include <vector>

#include "catalogue.hpp"

namespace catalist {

// Runs `catalist <args>` for a non-empty argument list and returns the exit
// code: 0 on success, 1 when the work failed, 2 when the command line is
// wrong. Lists move in and out as MyAnimeList XML files.
int runCli(const std::vector<std::string>& args, catalogue& db, std::ostream& out,
           std::ostream& err);

}  // namespace catalist

#endif  // CATALIST_CLI_HPP_
