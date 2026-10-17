#include "catalist/entry.hpp"
#include <algorithm>

namespace catalist {

std::string statusToString(WatchStatus status, MediaType type) {
  switch (status) {
    case WatchStatus::Watching:    return (type == MediaType::Manga) ? "Reading" : "Watching";
    case WatchStatus::Completed:   return "Completed";
    case WatchStatus::OnHold:      return "On-Hold";
    case WatchStatus::Dropped:     return "Dropped";
    case WatchStatus::PlanToWatch: return (type == MediaType::Manga) ? "Plan to Read" : "Plan to Watch";
  }
  return "Unknown";
}

std::string statusToIcon(WatchStatus status) {
  switch (status) {
    case WatchStatus::Watching:    return icon::Watching;
    case WatchStatus::Completed:   return icon::Completed;
    case WatchStatus::OnHold:      return icon::OnHold;
    case WatchStatus::Dropped:     return icon::Dropped;
    case WatchStatus::PlanToWatch: return icon::Plan;
  }
  return icon::Plan;
}

ftxui::Color statusToColor(WatchStatus status) {
  switch (status) {
    case WatchStatus::Watching:    return theme::Green;
    case WatchStatus::Completed:   return theme::Mauve;
    case WatchStatus::OnHold:      return theme::Yellow;
    case WatchStatus::Dropped:     return theme::Red;
    case WatchStatus::PlanToWatch: return theme::Overlay0;
  }
  return theme::Subtext;
}

WatchStatus stringToStatus(const std::string& str) {
  std::string lower = str;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

  if (lower.find("watch") != std::string::npos || lower.find("read") != std::string::npos || lower == "1") {
    return WatchStatus::Watching;
  } else if (lower.find("complete") != std::string::npos || lower == "2") {
    return WatchStatus::Completed;
  } else if (lower.find("hold") != std::string::npos || lower == "3") {
    return WatchStatus::OnHold;
  } else if (lower.find("drop") != std::string::npos || lower == "4") {
    return WatchStatus::Dropped;
  } else {
    return WatchStatus::PlanToWatch;
  }
}

std::string typeToString(MediaType type) {
  switch (type) {
    case MediaType::Anime: return "Anime";
    case MediaType::Manga: return "Manga";
  }
  return "Anime";
}

std::string typeToIcon(MediaType type) {
  switch (type) {
    case MediaType::Anime: return icon::Anime;
    case MediaType::Manga: return icon::Manga;
  }
  return icon::Anime;
}

ftxui::Color typeToColor(MediaType type) {
  switch (type) {
    case MediaType::Anime: return theme::Blue;
    case MediaType::Manga: return theme::Peach;
  }
  return theme::Blue;
}

MediaType stringToType(const std::string& str) {
  std::string lower = str;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
  if (lower.find("manga") != std::string::npos) {
    return MediaType::Manga;
  }
  return MediaType::Anime;
}

std::string renderStarMeter(int score) {
  score = std::clamp(score, 0, 10);
  int full = score / 2;
  bool half = (score % 2 == 1);
  int empty = 5 - full - (half ? 1 : 0);

  std::string meter = "";
  for (int i = 0; i < full; ++i) {
    if (!meter.empty()) meter += " ";
    meter += icon::StarFull;
  }
  if (half) {
    if (!meter.empty()) meter += " ";
    meter += icon::StarHalf;
  }
  for (int i = 0; i < empty; ++i) {
    if (!meter.empty()) meter += " ";
    meter += icon::StarEmpty;
  }
  return meter;
}

std::string escapeCsv(const std::string& field) {
  bool needsQuotes = false;
  for (char c : field) {
    if (c == ',' || c == '"' || c == '\n' || c == '\r') {
      needsQuotes = true;
      break;
    }
  }
  if (!needsQuotes) return field;

  std::string escaped = "\"";
  for (char c : field) {
    if (c == '"') escaped += "\"\"";
    else escaped += c;
  }
  escaped += "\"";
  return escaped;
}

std::string unescapeCsv(const std::string& field) {
  if (field.size() >= 2 && field.front() == '"' && field.back() == '"') {
    std::string unquoted = field.substr(1, field.size() - 2);
    std::string res = "";
    for (size_t i = 0; i < unquoted.size(); ++i) {
      if (unquoted[i] == '"' && i + 1 < unquoted.size() && unquoted[i + 1] == '"') {
        res += '"';
        ++i;
      } else {
        res += unquoted[i];
      }
    }
    return res;
  }
  return field;
}

} // namespace catalist
