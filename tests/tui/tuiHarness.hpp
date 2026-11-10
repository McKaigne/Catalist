#ifndef CATALIST_TESTS_TUI_TUI_HARNESS_HPP_
#define CATALIST_TESTS_TUI_TUI_HARNESS_HPP_

#include <ftxui/component/event.hpp>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "catalist/tui/appContext.hpp"
#include "catalist/tui/tab.hpp"
#include "support/testSupport.hpp"

namespace catalist::test {

// A complete appContext over fakes: in-memory files under /data, the fixed
// test clock, mock HTTP, a recording process launcher and no real network.
struct tuiHarness {
  tuiHarness();

  bool press(iTab& tab, const ftxui::Event& event);
  bool type(iTab& tab, const std::string& keys);
  [[nodiscard]] std::string screen(iTab& tab);

  memoryFileSystem fs;
  fixedClock clock;
  database db;
  appSettings settings;
  undoStack undo;
  notifier notices;
  fakeProcessLauncher launcher;
  std::shared_ptr<mockHttpClient> http;
  providerRegistry providers;
  accountFakes fakes;
  // After the fakes, so a running job is joined before they go away.
  jobRunner jobs;
  appContext ctx;
};

}  // namespace catalist::test

#endif  // CATALIST_TESTS_TUI_TUI_HARNESS_HPP_
