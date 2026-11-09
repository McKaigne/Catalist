#ifndef CATALIST_ENTRY_HPP_
#define CATALIST_ENTRY_HPP_

#include <string>
#include <unordered_map>

#include "catalist/theme.hpp"

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
  std::string startDate;
  std::string finishDate;
  std::string notes;
  std::string updatedAt;
  bool dirty = false;

  std::unordered_map<std::string, std::string> remoteIds = {};

  std::string getRemoteId(const std::string& provider) const;
  void setRemoteId(const std::string& provider, const std::string& remoteId);
};

std::string statusToString(watchStatus status,
                           mediaType type = mediaType::kAnime);
std::string statusToIcon(watchStatus status);
ftxui::Color statusToColor(watchStatus status);
watchStatus stringToStatus(const std::string& str);

std::string typeToString(mediaType type);
std::string typeToIcon(mediaType type);
ftxui::Color typeToColor(mediaType type);
mediaType stringToType(const std::string& str);

std::string renderStarMeter(int score);

std::string escapeCsv(const std::string& field);
std::string unescapeCsv(const std::string& field);
std::string serializeRemoteIds(
    const std::unordered_map<std::string, std::string>& ids);
std::unordered_map<std::string, std::string> deserializeRemoteIds(
    const std::string& s);

}  // namespace catalist

#endif  // CATALIST_ENTRY_HPP_
