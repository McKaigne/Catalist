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

std::optional<watchStatus> parseStatus(const std::string& str) {
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
  return std::nullopt;
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

std::string formatDate(const std::optional<isoDate>& date) {
  return date ? date->toString() : "";
}

std::string serializeServiceSet(const std::set<std::string>& services) {
  std::string text;
  for (const auto& service : services)
    text += (text.empty() ? "" : ";") + service;
  return text;
}

std::set<std::string> deserializeServiceSet(const std::string& text) {
  std::set<std::string> services;
  size_t start = 0;
  while (start <= text.size()) {
    size_t end = text.find(';', start);
    std::string service =
        text.substr(start, end == std::string::npos ? std::string::npos : end - start);
    if (!service.empty())
      services.insert(service);
    if (end == std::string::npos)
      break;
    start = end + 1;
  }
  return services;
}

}  // namespace catalist
