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
  int full = score / 2;
  bool half = (score % 2 == 1);
  int empty = 5 - full - (half ? 1 : 0);

  std::string meter = "";
  for (int i = 0; i < full; ++i) {
    if (!meter.empty())
      meter += " ";
    meter += icon::kStarFull;
  }
  if (half) {
    if (!meter.empty())
      meter += " ";
    meter += icon::kStarHalf;
  }
  for (int i = 0; i < empty; ++i) {
    if (!meter.empty())
      meter += " ";
    meter += icon::kStarEmpty;
  }
  return meter;
}

}  // namespace catalist
