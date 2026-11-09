#ifndef CATALIST_TUI_JOB_RUNNER_HPP_
#define CATALIST_TUI_JOB_RUNNER_HPP_

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

#include "catalist/tui/widgets/notifier.hpp"

namespace catalist {

// Runs one slow job (metadata sync, MAL login) at a time on a worker thread
// so the UI stays responsive; the job's returned message becomes a popup.
// The thread is joined on destruction so it never outlives what it uses.
class jobRunner {
public:
  jobRunner(notifier& notices, std::function<void()> wakeUi);
  ~jobRunner();
  jobRunner(const jobRunner&) = delete;
  jobRunner& operator=(const jobRunner&) = delete;

  // Returns false while another job is still running.
  bool start(std::string name, std::function<std::string()> job);
  [[nodiscard]] bool isRunning() const { return running_.load(); }
  [[nodiscard]] std::string runningName() const;

private:
  notifier& notices_;
  std::function<void()> wakeUi_;
  std::atomic<bool> running_{false};
  mutable std::mutex nameMutex_;
  std::string runningName_;
  std::thread worker_;
};

}  // namespace catalist

#endif  // CATALIST_TUI_JOB_RUNNER_HPP_
