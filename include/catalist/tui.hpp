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
  Nor,  // Normal modeless browsing
  Sch,  // Active search / filter typing
  Edt,  // Inline column / field cell editing
  Cmd,  // Floating command palette modal
  Vis   // Visual multi-selection mode
};

enum class editField { Status, Rating, Progress, Title, Notes };

enum class viewTab {
  About,     // 0: Catalist
  Anime,     // 1: Anime
  Manga,     // 2: Manga
  Overview,  // 3: MAL Statistics Replica
  Sync,      // 4: Upstream Sync & Manual Backups
  Settings   // 5: Settings & Appearance
};

enum class focusedCard { Sidebar, Catalog, Details };

struct historyAction {
  enum type { Mutate, Delete, Add } actionType;
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
