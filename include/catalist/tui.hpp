#pragma once

#include "catalist/commandPalette.hpp"
#include "catalist/config.hpp"
#include "catalist/database.hpp"
#include "catalist/themeRegistry.hpp"
#include <set>
#include <string>
#include <vector>

namespace catalist {

enum class appMode {
  kNor,  // Normal modeless browsing
  kSch,  // Active search / filter typing
  kEdt,  // Inline column / field cell editing
  kCmd,  // Floating command palette modal
  kVis   // Visual multi-selection mode
};

enum class editField { kStatus, kRating, kProgress, kTitle, kNotes };

enum class viewTab {
  kAbout,     // 0: Catalist
  kAnime,     // 1: Anime
  kManga,     // 2: Manga
  kOverview,  // 3: MAL Statistics Replica
  kSync,      // 4: Upstream Sync & Manual Backups
  kSettings   // 5: Settings & Appearance
};

enum class focusedCard { kSidebar, kCatalog, kDetails };

struct historyAction {
  enum type { kMutate, kDelete, kAdd } actionType;
  mediaType media;
  entry before;
  entry after;
};

class appTui {
 public:
  appTui(database& db, appConfig& config);
  void run();

 private:
  database& db_;
  appConfig& config_;
  commandPalette commandPalette_;
  std::vector<historyAction> undoStack_;
  std::vector<historyAction> redoStack_;
};

}  // namespace catalist
