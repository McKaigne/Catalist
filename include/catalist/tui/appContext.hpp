#ifndef CATALIST_TUI_APP_CONTEXT_HPP_
#define CATALIST_TUI_APP_CONTEXT_HPP_

#include <functional>
#include <string>

#include "catalist/config/config.hpp"
#include "catalist/core/clock.hpp"
#include "catalist/model/undoStack.hpp"
#include "catalist/net/malAuth.hpp"
#include "catalist/net/providers/providerRegistry.hpp"
#include "catalist/platform/fileSystem.hpp"
#include "catalist/platform/httpClient.hpp"
#include "catalist/platform/processLauncher.hpp"
#include "catalist/storage/database.hpp"
#include "catalist/tui/jobRunner.hpp"
#include "catalist/tui/widgets/notifier.hpp"

namespace catalist {

// Everything a tab may touch. Built once by the app shell; tabs never reach
// for globals.
struct appContext {
  database& db;
  appConfig& config;
  undoStack& undo;
  notifier& notices;
  iProcessLauncher& launcher;
  const iClock& clock;
  iFileSystem& fileSystem;
  iHttpClient& http;
  providerRegistry& providers;
  malAuth& auth;
  jobRunner& jobs;
  // Switches the shell to another tab by id (e.g. Catalog 'a' -> Search).
  std::function<void(const std::string& tabId)> openTab;
};

void saveConfig(appContext& ctx);

}  // namespace catalist

#endif  // CATALIST_TUI_APP_CONTEXT_HPP_
