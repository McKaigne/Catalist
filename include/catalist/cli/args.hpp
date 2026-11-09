#ifndef CATALIST_CLI_ARGS_HPP_
#define CATALIST_CLI_ARGS_HPP_

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "catalist/core/date.hpp"
#include "catalist/core/result.hpp"
#include "catalist/storage/database.hpp"

namespace catalist::cli {

enum class outputFormat { kTable, kPlain, kIds, kCsv, kJson };

// An inclusive score window; "unrated" is the 0..0 window.
struct ratingFilter {
  int minScore = 0;
  int maxScore = 10;
  bool unratedOnly = false;
};

struct cliFilterOptions {
  std::optional<mediaType> type;
  std::optional<watchStatus> status;
  std::optional<ratingFilter> rating;
  std::optional<std::string> year;
  std::string search;
  sortKey sortBy = sortKey::kScore;
  bool reverseSort = false;
  std::optional<size_t> limit;
  outputFormat format = outputFormat::kTable;
  bool jsonErrors = false;
};

struct argumentError {
  std::string message;
  bool jsonErrors = false;
};

// Accepts 9, >=9, >8, <=7, <8, 6..8, unrated (or 0); quotes and whitespace
// around the spec are ignored.
[[nodiscard]] std::optional<ratingFilter> parseRatingSpec(
    const std::string& spec);

[[nodiscard]] std::optional<mediaType> parseMediaTypeArgument(
    const std::string& raw);

[[nodiscard]] result<cliFilterOptions, argumentError> parseCommonFilters(
    const std::vector<std::string>& args, cliFilterOptions defaults = {});

[[nodiscard]] std::vector<entry> collectAndFilterEntries(
    const database& db, const cliFilterOptions& opts);

// "-" reads one target per stdin line (trimmed, blanks skipped).
[[nodiscard]] std::vector<std::string> collectQueries(const std::string& target);

// An empty value clears the date; anything else must be YYYY-MM-DD. On a
// malformed value the usage error is printed and nullopt returned.
[[nodiscard]] std::optional<std::optional<isoDate>> parseDateArgument(
    const std::string& raw, const std::string& flag, bool jsonMode);

}  // namespace catalist::cli

#endif  // CATALIST_CLI_ARGS_HPP_
