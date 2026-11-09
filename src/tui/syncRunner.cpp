#include "catalist/tui/syncRunner.hpp"

#include "catalist/net/metadataSync.hpp"

namespace catalist {

syncRunner::syncRunner(database& db, iHttpClient& http, notifier& notices,
                       std::function<void()> wakeUi)
    : db_(db), http_(http), notices_(notices), wakeUi_(std::move(wakeUi)) {}

syncRunner::~syncRunner() {
  if (worker_.joinable())
    worker_.join();
}

bool syncRunner::start() {
  if (running_.exchange(true))
    return false;
  if (worker_.joinable())
    worker_.join();
  worker_ = std::thread([this]() {
    std::string outcome;
    metadataSync::syncOngoing(db_, http_, outcome);
    running_.store(false);
    notices_.push(outcome);
    wakeUi_();
  });
  return true;
}

}  // namespace catalist
