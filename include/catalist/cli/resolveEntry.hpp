#ifndef CATALIST_CLI_RESOLVE_ENTRY_HPP_
#define CATALIST_CLI_RESOLVE_ENTRY_HPP_

#include <string>
#include <vector>

#include "catalist/core/result.hpp"
#include "catalist/database.hpp"

namespace catalist {

enum class resolveFailure { kNotFound, kAmbiguous };

struct resolveError {
  resolveFailure kind = resolveFailure::kNotFound;
  std::string query;
  std::vector<entry> candidates;
};

// Resolves a CLI target the way users type it: a local or MAL id first, then
// an exact title (case-insensitive), then a unique title substring. Anime is
// searched before manga, and an exact manga title still beats anime
// substrings.
[[nodiscard]] result<entry, resolveError> resolveEntry(
    const database& db, const std::string& query);

}  // namespace catalist

#endif  // CATALIST_CLI_RESOLVE_ENTRY_HPP_
