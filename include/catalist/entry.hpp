#pragma once

#include <string>
#include <unordered_map>

#include "catalist/theme.hpp"

namespace catalist {

enum class MediaType { Anime, Manga };

enum class WatchStatus { Watching, Completed, OnHold, Dropped, PlanToWatch };

struct Entry {
  int id = 0;
  int malId = 0;
  int anilistId = 0;
  std::string title;
  MediaType type = MediaType::Anime;
  WatchStatus status = WatchStatus::Watching;
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

std::string statusToString(WatchStatus status,
                           MediaType type = MediaType::Anime);
std::string statusToIcon(WatchStatus status);
ftxui::Color statusToColor(WatchStatus status);
WatchStatus stringToStatus(const std::string& str);

std::string typeToString(MediaType type);
std::string typeToIcon(MediaType type);
ftxui::Color typeToColor(MediaType type);
MediaType stringToType(const std::string& str);

std::string renderStarMeter(int score);

std::string escapeCsv(const std::string& field);
std::string unescapeCsv(const std::string& field);
std::string serializeRemoteIds(
    const std::unordered_map<std::string, std::string>& ids);
std::unordered_map<std::string, std::string> deserializeRemoteIds(
    const std::string& s);

}  // namespace catalist
