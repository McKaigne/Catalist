#include "catalist/tui/jobRunner.hpp"

namespace catalist {

jobRunner::jobRunner(notifier& notices, std::function<void()> wakeUi)
    : notices_(notices), wakeUi_(std::move(wakeUi)) {}

jobRunner::~jobRunner() {
  if (worker_.joinable())
    worker_.join();
}

bool jobRunner::start(std::string name, std::function<std::string()> job) {
  if (running_.exchange(true))
    return false;
  if (worker_.joinable())
    worker_.join();
  {
    std::lock_guard<std::mutex> lock(nameMutex_);
    runningName_ = std::move(name);
  }
  worker_ = std::thread([this, job = std::move(job)]() {
    std::string outcome = job();
    running_.store(false);
    notices_.push(outcome);
    wakeUi_();
  });
  return true;
}

std::string jobRunner::runningName() const {
  std::lock_guard<std::mutex> lock(nameMutex_);
  return running_.load() ? runningName_ : "";
}

}  // namespace catalist
