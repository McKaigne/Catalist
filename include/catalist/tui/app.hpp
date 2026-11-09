#ifndef CATALIST_TUI_APP_HPP_
#define CATALIST_TUI_APP_HPP_

#include <optional>
#include <string>
#include <vector>

#include "catalist/config/config.hpp"
#include "catalist/core/clock.hpp"
#include "catalist/model/undoStack.hpp"
#include "catalist/net/malAuth.hpp"
#include "catalist/net/providers/providerRegistry.hpp"
#include "catalist/platform/fileSystem.hpp"
#include "catalist/platform/httpClient.hpp"
#include "catalist/platform/processLauncher.hpp"
#include "catalist/storage/database.hpp"
#include "catalist/tui/widgets/commandPalette.hpp"
#include "catalist/tui/widgets/notifier.hpp"

namespace catalist {

struct tuiServices {
  iFileSystem& fileSystem;
  iProcessLauncher& launcher;
  const iClock& clock;
  iHttpClient& http;
  providerRegistry& providers;
  malAuth& auth;
};

// Ids of the tabs this build provides, in default order.
[[nodiscard]] std::vector<std::string> builtInTabIds();

class appTui {
 public:
  appTui(database& db, appConfig& config, tuiServices services);
  // startupTab overrides the configured startup tab (from --tab).
  void run(const std::optional<std::string>& startupTab = std::nullopt);

 private:
  database& db_;
  appConfig& config_;
  tuiServices services_;
  undoStack undo_;
  notifier notices_;
  commandPalette palette_;
};

}  // namespace catalist

#endif  // CATALIST_TUI_APP_HPP_
