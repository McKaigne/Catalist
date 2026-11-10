#ifndef CATALIST_CLI_COMMAND_CONTEXT_HPP_
#define CATALIST_CLI_COMMAND_CONTEXT_HPP_

#include <cstddef>
#include <filesystem>
#include <functional>

#include "catalist/settings/settings.hpp"
#include "catalist/core/clock.hpp"
#include "catalist/platform/fileSystem.hpp"
#include "catalist/net/accounts.hpp"
#include "catalist/platform/httpClient.hpp"
#include "catalist/platform/processLauncher.hpp"
#include "catalist/storage/database.hpp"

namespace catalist::cli {

// Everything a command may use; built by main.cpp (or a test) and passed by
// reference, so commands never construct their own services.
struct commandContext {
  database& db;
  appSettings& settings;
  const iClock& clock;
  iHttpClient& http;
  iFileSystem& fileSystem;
  iProcessLauncher& launcher;
  listAccounts& accounts;
  std::filesystem::path settingsPath;
  // Returns an index in [0, count); injectable so `random` is testable.
  std::function<size_t(size_t count)> randomIndex;
  // Whether stdout is a terminal: tables get rules and a footer, colour is
  // allowed and confirmation prompts may be shown.
  bool interactive = false;
};

[[nodiscard]] size_t pickRandomIndex(size_t count);

}  // namespace catalist::cli

#endif  // CATALIST_CLI_COMMAND_CONTEXT_HPP_
