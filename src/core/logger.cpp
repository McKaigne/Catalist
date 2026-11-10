#include "catalist/core/logger.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace catalist {

namespace {

const char* levelToString(logLevel level) {
  switch (level) {
    case logLevel::kDebug: return "DEBUG";
    case logLevel::kInfo:  return "INFO ";
    case logLevel::kWarn:  return "WARN ";
    case logLevel::kError: return "ERROR";
  }
  return "INFO ";
}

std::string formatTimestamp(const iClock& clock) {
  auto sinceEpoch = clock.now().time_since_epoch();
  auto millis =
      std::chrono::duration_cast<std::chrono::milliseconds>(sinceEpoch) % 1000;
  std::ostringstream ss;
  ss << formatLocalTime(clock, "%Y-%m-%d %H:%M:%S") << "."
     << std::setfill('0') << std::setw(3) << millis.count();
  return ss.str();
}

}  // namespace

logger& logger::get() {
  static logger instance;
  return instance;
}

void logger::init(const std::filesystem::path& logPath,
                  iFileSystem& fileSystem, const iClock& clock) {
  std::lock_guard<std::mutex> lock(mutex_);
  logPath_ = logPath;
  fileSystem_ = &fileSystem;
  clock_ = &clock;
}

void logger::log(logLevel level, std::string_view message) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!fileSystem_ || !clock_)
    return;
  std::string line = "[" + formatTimestamp(*clock_) + "] [" +
                     levelToString(level) + "] " + std::string(message) + "\n";
  [[maybe_unused]] bool written = fileSystem_->appendFile(logPath_, line);
}

void logger::debug(std::string_view message) {
  get().log(logLevel::kDebug, message);
}

void logger::info(std::string_view message) {
  get().log(logLevel::kInfo, message);
}

void logger::warn(std::string_view message) {
  get().log(logLevel::kWarn, message);
}

void logger::error(std::string_view message) {
  get().log(logLevel::kError, message);
}

}  // namespace catalist
