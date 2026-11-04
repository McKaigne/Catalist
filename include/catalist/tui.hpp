#pragma once

#include "catalist/commandPalette.hpp"
#include "catalist/config.hpp"
#include "catalist/database.hpp"
#include "catalist/themeRegistry.hpp"
#include <set>
#include <string>
#include <vector>

namespace catalist {

enum class AppMode {
  Nor,  // Normal modeless browsing
  Sch,  // Active search / filter typing
  Edt,  // Inline column / field cell editing
  Cmd,  // Floating command palette modal
  Vis   // Visual multi-selection mode
};

enum class EditField { Status, Rating, Progress, Title, Notes };

enum class ViewTab {
  About,     // 0: Catalist
  Anime,     // 1: Anime
  Manga,     // 2: Manga
  Overview,  // 3: MAL Statistics Replica
  Sync,      // 4: Upstream Sync & Manual Backups
  Settings   // 5: Settings & Appearance
};

enum class FocusedCard { Sidebar, Catalog, Details };

struct HistoryAction {
  enum Type { Mutate, Delete, Add } type;
  MediaType media;
  Entry before;
  Entry after;
};

class AppTui {
 public:
  AppTui(Database& db, AppConfig& config);
  void run();

 private:
  Database& db_;
  AppConfig& config_;
  CommandPalette commandPalette_;
  std::vector<HistoryAction> undoStack_;
  std::vector<HistoryAction> redoStack_;
};

}  // namespace catalist
