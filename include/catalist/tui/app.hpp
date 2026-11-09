#pragma once

#include "catalist/tui/widgets/commandPalette.hpp"
#include "catalist/config/config.hpp"
#include "catalist/core/clock.hpp"
#include "catalist/storage/database.hpp"
#include "catalist/model/undoStack.hpp"
#include "catalist/platform/fileSystem.hpp"
#include "catalist/platform/httpClient.hpp"
#include "catalist/platform/processLauncher.hpp"
#include "catalist/net/providers/providerRegistry.hpp"
#include "catalist/net/malAuth.hpp"
#include "catalist/tui/theme/themeRegistry.hpp"
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


struct tuiServices {
  iFileSystem& fileSystem;
  iProcessLauncher& launcher;
  const iClock& clock;
  iHttpClient& http;
  providerRegistry& providers;
  malAuth& auth;
};

class appTui {
 public:
  appTui(database& db, appConfig& config, tuiServices services);
  void run();

 private:
  database& db_;
  appConfig& config_;
  tuiServices services_;
  commandPalette commandPalette_;
  undoStack undo_;
};

}  // namespace catalist
