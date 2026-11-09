#include "catalist/model/entry.hpp"

#include <algorithm>

namespace catalist {

std::string statusToString(watchStatus status, mediaType type) {
  switch (status) {
    case watchStatus::kWatching:
      return (type == mediaType::kManga) ? "Reading" : "Watching";
    case watchStatus::kCompleted:
      return "Completed";
    case watchStatus::kOnHold:
      return "On-Hold";
    case watchStatus::kDropped:
      return "Dropped";
    case watchStatus::kPlanToWatch:
      return (type == mediaType::kManga) ? "Plan to Read" : "Plan to Watch";
  }
  return "Unknown";
}

watchStatus stringToStatus(const std::string& str) {
  std::string lower = str;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

  if (lower.find("plan") != std::string::npos || lower == "5" ||
      lower == "plan-to-watch" || lower == "plan-to-read") {
    return watchStatus::kPlanToWatch;
  } else if (lower.find("watch") != std::string::npos ||
             lower.find("read") != std::string::npos || lower == "1") {
    return watchStatus::kWatching;
  } else if (lower.find("complete") != std::string::npos || lower == "2") {
    return watchStatus::kCompleted;
  } else if (lower.find("hold") != std::string::npos || lower == "3") {
    return watchStatus::kOnHold;
  } else if (lower.find("drop") != std::string::npos || lower == "4") {
    return watchStatus::kDropped;
  }
  return watchStatus::kPlanToWatch;
}

std::string typeToString(mediaType type) {
  switch (type) {
    case mediaType::kAnime:
      return "Anime";
    case mediaType::kManga:
      return "Manga";
  }
  return "Anime";
}

mediaType stringToType(const std::string& str) {
  std::string lower = str;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
  if (lower.find("manga") != std::string::npos) {
    return mediaType::kManga;
  }
  return mediaType::kAnime;
}

std::string escapeCsv(const std::string& field) {
  bool needsQuotes = false;
  for (char c : field) {
    if (c == ',' || c == '"' || c == '\n' || c == '\r') {
      needsQuotes = true;
      break;
    }
  }
  if (!needsQuotes)
    return field;

  std::string escaped = "\"";
  for (char c : field) {
    if (c == '"')
      escaped += "\"\"";
    else
      escaped += c;
  }
  escaped += "\"";
  return escaped;
}

std::string entry::getRemoteId(const std::string& provider) const {
  auto it = remoteIds.find(provider);
  if (it != remoteIds.end())
    return it->second;
  if (provider == "mal" && malId > 0)
    return std::to_string(malId);
  if (provider == "anilist" && anilistId > 0)
    return std::to_string(anilistId);
  return "";
}

void entry::setRemoteId(const std::string& provider,
                        const std::string& remoteId) {
  if (remoteId.empty()) {
    remoteIds.erase(provider);
    if (provider == "mal")
      malId = 0;
    if (provider == "anilist")
      anilistId = 0;
    return;
  }
  remoteIds[provider] = remoteId;
  try {
    if (provider == "mal")
      malId = std::stoi(remoteId);
    else if (provider == "anilist")
      anilistId = std::stoi(remoteId);
  } catch (...) {
  }
}

std::string serializeRemoteIds(
    const std::unordered_map<std::string, std::string>& ids) {
  std::string res;
  for (const auto& [k, v] : ids) {
    if (k.empty() || v.empty())
      continue;
    if (!res.empty())
      res += ";";
    res += k + "=" + v;
  }
  return res;
}

std::unordered_map<std::string, std::string> deserializeRemoteIds(
    const std::string& s) {
  std::unordered_map<std::string, std::string> res;
  if (s.empty())
    return res;
  size_t start = 0;
  while (start < s.size()) {
    size_t semi = s.find(';', start);
    if (semi == std::string::npos)
      semi = s.size();
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
      if (unquoted[i] == '"' && i + 1 < unquoted.size() &&
          unquoted[i + 1] == '"') {
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

}  // namespace catalist
