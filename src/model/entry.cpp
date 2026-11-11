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

  if (lower.find("plan") != std::string::npos || lower == "5") {
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

std::optional<isoDate> listDate(const entry& shown) {
  if (shown.status == watchStatus::kCompleted && shown.finishDate)
    return shown.finishDate;
  return shown.startDate ? shown.startDate : shown.updatedAt;
}

}  // namespace catalist
