#include "catalist/tui/theme/statusStyle.hpp"

#include <algorithm>

#include "catalist/tui/theme/palettes.hpp"

namespace catalist {

std::string statusToIcon(watchStatus status) {
  switch (status) {
    case watchStatus::kWatching:
      return icon::kWatching;
    case watchStatus::kCompleted:
      return icon::kCompleted;
    case watchStatus::kOnHold:
      return icon::kOnHold;
    case watchStatus::kDropped:
      return icon::kDropped;
    case watchStatus::kPlanToWatch:
      return icon::kPlan;
  }
  return icon::kPlan;
}

ftxui::Color statusToColor(watchStatus status) {
  switch (status) {
    case watchStatus::kWatching:
      return theme::kGreen;
    case watchStatus::kCompleted:
      return theme::kMauve;
    case watchStatus::kOnHold:
      return theme::kYellow;
    case watchStatus::kDropped:
      return theme::kRed;
    case watchStatus::kPlanToWatch:
      return theme::kOverlay0;
  }
  return theme::kSubtext;
}

std::string typeToIcon(mediaType type) {
  switch (type) {
    case mediaType::kAnime:
      return icon::kAnime;
    case mediaType::kManga:
      return icon::kManga;
  }
  return icon::kAnime;
}

ftxui::Color typeToColor(mediaType type) {
  switch (type) {
    case mediaType::kAnime:
      return theme::kBlue;
    case mediaType::kManga:
      return theme::kPeach;
  }
  return theme::kBlue;
}

std::string renderStarMeter(int score) {
  score = std::clamp(score, 0, 10);
  std::string meter;
  // Each of the five stars stands for two points.
  for (int star = 0; star < 5; ++star) {
    int points = score - star * 2;
    meter += star > 0 ? " " : "";
    meter += points >= 2   ? icon::kStarFull
             : points == 1 ? icon::kStarHalf
                           : icon::kStarEmpty;
  }
  return meter;
}

}  // namespace catalist
