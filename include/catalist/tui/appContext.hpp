#ifndef CATALIST_TUI_APP_CONTEXT_HPP_
#define CATALIST_TUI_APP_CONTEXT_HPP_

#include <string>

#include "catalist/core/clock.hpp"
#include "catalist/platform/httpClient.hpp"
#include "catalist/storage/database.hpp"

namespace catalist {

// Everything a tab may touch. Built once by runTui; tabs never reach for
// globals.
struct appContext {
  database& db;
  const iClock& clock;
  iHttpClient& http;
  // The message in the bottom bar.
  std::string status = "Ready.";

  // Writes the catalogue after a change, then shows `done`, or the failure.
  void save(const std::string& done);
};

}  // namespace catalist

#endif  // CATALIST_TUI_APP_CONTEXT_HPP_
