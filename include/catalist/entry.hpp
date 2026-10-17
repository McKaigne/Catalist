#pragma once

#include "catalist/theme.hpp"
#include <string>

namespace catalist {

enum class MediaType {
  Anime,
  Manga
};

enum class WatchStatus {
  Watching,
  Completed,
  OnHold,
  Dropped,
  PlanToWatch
};

struct Entry {
  int id = 0;              // Canonical primary key (matches malId or local sequence)
  int malId = 0;           // Official MyAnimeList database ID
  int anilistId = 0;       // Reserved for AniList sync
  std::string title;
  MediaType type = MediaType::Anime;
  WatchStatus status = WatchStatus::Watching;
  int score = 0;           // 0 to 10 scale (mapped to 0.0 - 5.0 stars)
  int progress = 0;        // Watched episodes or read chapters
  int totalUnits = 0;      // Total episodes or chapters (0 = ongoing / ?)
  int volumes = 0;         // Volumes read (for manga)
  int totalVolumes = 0;    // Total volumes
  int rewatched = 0;       // Rewatch / reread count
  std::string startDate;
  std::string finishDate;
  std::string notes;
  std::string updatedAt;
  bool dirty = false;
};

std::string statusToString(WatchStatus status, MediaType type = MediaType::Anime);
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

} // namespace catalist
