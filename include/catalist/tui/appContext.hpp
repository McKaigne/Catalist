#ifndef CATALIST_TUI_APP_CONTEXT_HPP_
#define CATALIST_TUI_APP_CONTEXT_HPP_

#include "catalist/core/clock.hpp"
#include "catalist/model/undoStack.hpp"
#include "catalist/platform/processLauncher.hpp"
#include "catalist/storage/database.hpp"
#include "catalist/tui/widgets/notifier.hpp"

namespace catalist {

// Everything a tab may touch. Built once by the app shell; tabs never reach
// for globals.
struct appContext {
  database& db;
  undoStack& undo;
  notifier& notices;
  iProcessLauncher& launcher;
  const iClock& clock;
};

}  // namespace catalist

#endif  // CATALIST_TUI_APP_CONTEXT_HPP_
