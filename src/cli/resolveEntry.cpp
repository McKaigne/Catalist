#include "catalist/cli/resolveEntry.hpp"

#include <algorithm>
#include <cctype>
#include <optional>

namespace catalist {

namespace {

std::string toLower(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return text;
}

bool isAllDigits(const std::string& text) {
  return !text.empty() &&
         std::all_of(text.begin(), text.end(),
                     [](unsigned char c) { return std::isdigit(c); });
}

std::optional<entry> findById(const database& db, const std::string& query) {
  if (!isAllDigits(query))
    return std::nullopt;
  int id = 0;
  try {
    id = std::stoi(query);
  } catch (...) {
    return std::nullopt;
  }
  if (auto anime = db.getById(mediaType::kAnime, id))
    return anime;
  return db.getById(mediaType::kManga, id);
}

// Returns true once an exact title match has replaced the candidate list.
bool collectTitleMatches(const database& db, mediaType type,
                         const std::string& loweredQuery,
                         std::vector<entry>& candidates) {
  for (const auto& candidate : db.getEntries(type)) {
    std::string loweredTitle = toLower(candidate.title);
    if (loweredTitle == loweredQuery) {
      candidates.assign(1, candidate);
      return true;
    }
    if (loweredTitle.find(loweredQuery) != std::string::npos)
      candidates.push_back(candidate);
  }
  return false;
}

}  // namespace

result<entry, resolveError> resolveEntry(const database& db,
                                         const std::string& query) {
  using outcome = result<entry, resolveError>;
  if (auto byId = findById(db, query))
    return outcome::success(*byId);

  std::string loweredQuery = toLower(query);
  std::vector<entry> candidates;
  if (!collectTitleMatches(db, mediaType::kAnime, loweredQuery, candidates))
    collectTitleMatches(db, mediaType::kManga, loweredQuery, candidates);

  if (candidates.size() == 1)
    return outcome::success(candidates.front());
  if (candidates.empty())
    return outcome::failure({resolveFailure::kNotFound, query, {}});
  return outcome::failure(
      {resolveFailure::kAmbiguous, query, std::move(candidates)});
}

}  // namespace catalist
