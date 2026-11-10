#ifndef CATALIST_CLI_HPP_
#define CATALIST_CLI_HPP_

#include <iosfwd>
#include <string>
#include <vector>

#include "catalist/platform/fileSystem.hpp"
#include "catalist/storage/database.hpp"

namespace catalist {

// Runs `catalist <args>` for a non-empty argument list and returns the exit
// code: 0 on success, 1 when the work failed, 2 when the command line is
// wrong. Lists move in and out as MyAnimeList XML files, see printUsage.
int runCli(const std::vector<std::string>& args, database& db,
           const iFileSystem& fileSystem, std::ostream& out, std::ostream& err);

}  // namespace catalist

#endif  // CATALIST_CLI_HPP_
