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

  if (lower.find("plan") != std::string::npos || lower == "5" || lower == "plan-to-watch" || lower == "plan-to-read") {
    return WatchStatus::PlanToWatch;
  } else if (lower.find("watch") != std::string::npos || lower.find("read") != std::string::npos || lower == "1") {
    return WatchStatus::Watching;
  } else if (lower.find("complete") != std::string::npos || lower == "2") {
    return WatchStatus::Completed;
  } else if (lower.find("hold") != std::string::npos || lower == "3") {
    return WatchStatus::OnHold;
  } else if (lower.find("drop") != std::string::npos || lower == "4") {
    return WatchStatus::Dropped;
  }
  return WatchStatus::PlanToWatch;
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

std::string Entry::getRemoteId(const std::string& provider) const {
  auto it = remoteIds.find(provider);
  if (it != remoteIds.end()) return it->second;
  if (provider == "mal" && malId > 0) return std::to_string(malId);
  if (provider == "anilist" && anilistId > 0) return std::to_string(anilistId);
  return "";
}

void Entry::setRemoteId(const std::string& provider, const std::string& remoteId) {
  if (remoteId.empty()) {
    remoteIds.erase(provider);
    if (provider == "mal") malId = 0;
    if (provider == "anilist") anilistId = 0;
    return;
  }
  remoteIds[provider] = remoteId;
  try {
    if (provider == "mal") malId = std::stoi(remoteId);
    else if (provider == "anilist") anilistId = std::stoi(remoteId);
  } catch (...) {}
}

std::string serializeRemoteIds(const std::unordered_map<std::string, std::string>& ids) {
  std::string res;
  for (const auto& [k, v] : ids) {
    if (k.empty() || v.empty()) continue;
    if (!res.empty()) res += ";";
    res += k + "=" + v;
  }
  return res;
}

std::unordered_map<std::string, std::string> deserializeRemoteIds(const std::string& s) {
  std::unordered_map<std::string, std::string> res;
  if (s.empty()) return res;
  size_t start = 0;
  while (start < s.size()) {
    size_t semi = s.find(';', start);
    if (semi == std::string::npos) semi = s.size();
    std::string pair = s.substr(start, semi - start);
    size_t eq = pair.find('=');
    if (eq != std::string::npos && eq > 0 && eq + 1 < pair.size()) {
      res[pair.substr(0, eq)] = pair.substr(eq + 1);
    }
    start = semi + 1;
  }
  return res;
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
