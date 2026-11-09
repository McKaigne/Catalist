#ifndef CATALIST_CLI_OUTPUT_HPP_
#define CATALIST_CLI_OUTPUT_HPP_

#include <string>
#include <vector>

#include "catalist/cli/resolveEntry.hpp"
#include "catalist/model/entry.hpp"

namespace catalist {

namespace ansi {
extern const std::string kReset;
extern const std::string kBold;
extern const std::string kDim;
extern const std::string kMauve;
extern const std::string kLavender;
extern const std::string kPeach;
extern const std::string kYellow;
extern const std::string kGreen;
extern const std::string kBlue;
extern const std::string kRed;
extern const std::string kSubtext;
}  // namespace ansi

}  // namespace catalist

// Output rules (ADR-004): tables and decorations only on a TTY, plain and
// machine formats otherwise; every diagnostic goes to stderr.
namespace catalist::cli {

inline const std::string kStatusChoices =
    "Expected: watching, reading, completed, on-hold, dropped, planned.";

[[nodiscard]] bool shouldColor(bool interactive);
void emitError(const std::string& code, const std::string& message,
               bool jsonMode);
void reportResolveError(const resolveError& failure, bool jsonMode,
                        bool listCandidates);

[[nodiscard]] std::string formatJsonEntry(const entry& e);
[[nodiscard]] std::string formatJsonEntries(const std::vector<entry>& entries);
void printSingleDetail(const entry& e, bool jsonMode, bool color);
void printUsage();

}  // namespace catalist::cli

#endif  // CATALIST_CLI_OUTPUT_HPP_
