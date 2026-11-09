#ifndef CATALIST_TUI_SYNC_RUNNER_HPP_
#define CATALIST_TUI_SYNC_RUNNER_HPP_

#include <atomic>
#include <functional>
#include <thread>

#include "catalist/platform/httpClient.hpp"
#include "catalist/storage/database.hpp"
#include "catalist/tui/widgets/notifier.hpp"

namespace catalist {

// Runs metadataSync on a worker thread so the UI stays responsive. The
// database and notifier are internally synchronised; the thread is joined on
// destruction so it never outlives them.
class syncRunner {
public:
  syncRunner(database& db, iHttpClient& http, notifier& notices,
             std::function<void()> wakeUi);
  ~syncRunner();
  syncRunner(const syncRunner&) = delete;
  syncRunner& operator=(const syncRunner&) = delete;

  // Returns false when a sync is already running.
  bool start();
  [[nodiscard]] bool isRunning() const { return running_.load(); }

private:
  database& db_;
  iHttpClient& http_;
  notifier& notices_;
  std::function<void()> wakeUi_;
  std::atomic<bool> running_{false};
  std::thread worker_;
};

}  // namespace catalist

#endif  // CATALIST_TUI_SYNC_RUNNER_HPP_
