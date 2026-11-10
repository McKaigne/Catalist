#ifndef CATALIST_ENTRY_HPP_
#define CATALIST_ENTRY_HPP_

#include <optional>
#include <string>

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
  // Position in the user's watch queue; absent until the user orders it.
  std::optional<int> queuePos;

};

std::string statusToString(watchStatus status,
                           mediaType type = mediaType::kAnime);
// Accepts the display names, CLI spellings and MAL's 1-5 codes; absent for
// anything else.
[[nodiscard]] std::optional<watchStatus> parseStatus(const std::string& str);

std::string typeToString(mediaType type);

// The display and file form of an optional date: ISO text, or empty.
[[nodiscard]] std::string formatDate(const std::optional<isoDate>& date);


}  // namespace catalist

#endif  // CATALIST_ENTRY_HPP_
