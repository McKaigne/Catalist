#include "catalist/core/logger.hpp"
#include "catalist/database.hpp"

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

// Messages logged before init() (or without a clock) still get a timestamp
// from the real clock rather than being dropped.
const iClock& fallbackClock() {
  static const realClock clock;
  return clock;
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

logger::logger() = default;

logger::~logger() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (logFile_.is_open()) {
    logFile_.flush();
    logFile_.close();
  }
}

void logger::init(const std::filesystem::path& logPath, const iClock& clock) {
  std::lock_guard<std::mutex> lock(mutex_);
  clock_ = &clock;
  if (logFile_.is_open()) {
    logFile_.flush();
    logFile_.close();
  }
  logPath_ = logPath;
  if (!logPath_.parent_path().empty()) {
    std::error_code ec;
    std::filesystem::create_directories(logPath_.parent_path(), ec);
  }
  logFile_.open(logPath_, std::ios::out | std::ios::app);
  initialized_ = true;
}

void logger::ensureOpen() {
  if (initialized_ && logFile_.is_open()) return;
  if (logPath_.empty()) {
    logPath_ = database::getConfigDir() / "catalist.log";
  }
  if (!logPath_.parent_path().empty()) {
    std::error_code ec;
    std::filesystem::create_directories(logPath_.parent_path(), ec);
  }
  logFile_.open(logPath_, std::ios::out | std::ios::app);
  initialized_ = true;
}

void logger::log(logLevel level, std::string_view message) {
  std::lock_guard<std::mutex> lock(mutex_);
  ensureOpen();
  if (!logFile_.is_open()) return;

  const iClock& clock = clock_ ? *clock_ : fallbackClock();
  logFile_ << "[" << formatTimestamp(clock) << "] ["
           << levelToString(level) << "] "
           << message << "\n";
  logFile_.flush();
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

std::filesystem::path logger::getLogPath() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return logPath_;
}

}  // namespace catalist
