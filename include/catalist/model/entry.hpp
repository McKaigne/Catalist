#ifndef CATALIST_ENTRY_HPP_
#define CATALIST_ENTRY_HPP_

#include <optional>
#include <string>
#include <unordered_map>

#include "catalist/core/date.hpp"


namespace catalist {

enum class mediaType { kAnime, kManga };

enum class watchStatus { kWatching, kCompleted, kOnHold, kDropped, kPlanToWatch };

struct entry {
  int id = 0;
  int malId = 0;
  int anilistId = 0;
  std::string title;
  mediaType type = mediaType::kAnime;
  watchStatus status = watchStatus::kWatching;
  int score = 0;
  int progress = 0;
  int totalUnits = 0;
  int volumes = 0;
  int totalVolumes = 0;
  int rewatched = 0;
  std::optional<isoDate> startDate;
  std::optional<isoDate> finishDate;
  std::string notes;
  std::optional<isoDate> updatedAt;
  bool dirty = false;
  // Position in the user's watch queue; absent until the user orders it.
  std::optional<int> queuePos;

  std::unordered_map<std::string, std::string> remoteIds = {};

  std::string getRemoteId(const std::string& provider) const;
  void setRemoteId(const std::string& provider, const std::string& remoteId);
};

std::string statusToString(watchStatus status,
                           mediaType type = mediaType::kAnime);
watchStatus stringToStatus(const std::string& str);

std::string typeToString(mediaType type);

// The display and file form of an optional date: ISO text, or empty.
[[nodiscard]] std::string formatDate(const std::optional<isoDate>& date);
mediaType stringToType(const std::string& str);

std::string serializeRemoteIds(
    const std::unordered_map<std::string, std::string>& ids);
std::unordered_map<std::string, std::string> deserializeRemoteIds(
    const std::string& s);

}  // namespace catalist

#endif  // CATALIST_ENTRY_HPP_
