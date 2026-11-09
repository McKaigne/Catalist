#ifndef CATALIST_NET_LIST_SERVICE_HPP_
#define CATALIST_NET_LIST_SERVICE_HPP_

#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "catalist/core/result.hpp"
#include "catalist/model/entry.hpp"

namespace catalist {

// One row of a user's list on a remote service, already in Catalist terms.
struct remoteListEntry {
  // Carries the remote ids (malId and, where the service knows it, anilistId),
  // the title and totals, and the user's status, score, progress, dates,
  // notes and rewatch count.
  entry item;
  // AniList's own id for the list row, needed to delete it; MAL has none.
  std::optional<long long> listEntryId;
};

using syncOutcome = result<std::monostate>;

// A user's anime and manga list on MyAnimeList or AniList.
class iListService {
public:
  virtual ~iListService() = default;

  // "mal" or "anilist"; also the key used in entry::syncedWith.
  [[nodiscard]] virtual std::string key() const = 0;
  [[nodiscard]] virtual std::string displayName() const = 0;
  [[nodiscard]] virtual bool isConnected() const = 0;

  // The whole list of the logged-in user, or of `userName` when given (a
  // public list, no login needed).
  [[nodiscard]] virtual result<std::vector<remoteListEntry>> fetchList(
      mediaType type, const std::optional<std::string>& userName) = 0;
  // Creates or updates the logged-in user's list row for `item`, which must
  // carry this service's id.
  [[nodiscard]] virtual syncOutcome saveEntry(const entry& item) = 0;
  [[nodiscard]] virtual syncOutcome deleteEntry(const remoteListEntry& row) = 0;

  // This service's id of `item`, if it has one.
  [[nodiscard]] virtual std::optional<int> idOf(const entry& item) const = 0;
};

// Translates MyAnimeList ids to AniList ids and back; AniList knows both.
class iIdMapper {
public:
  virtual ~iIdMapper() = default;
  // Ids with no counterpart are left out of the map.
  [[nodiscard]] virtual result<std::map<int, int>> anilistIdsForMal(
      mediaType type, const std::vector<int>& malIds) = 0;
  [[nodiscard]] virtual result<std::map<int, int>> malIdsForAnilist(
      mediaType type, const std::vector<int>& anilistIds) = 0;
};

[[nodiscard]] inline syncOutcome syncSucceeded() {
  return syncOutcome::success(std::monostate{});
}

}  // namespace catalist

#endif  // CATALIST_NET_LIST_SERVICE_HPP_
