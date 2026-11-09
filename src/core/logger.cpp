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
    case logLevel::Debug: return "DEBUG";
    case logLevel::Info:  return "INFO ";
    case logLevel::Warn:  return "WARN ";
    case logLevel::Error: return "ERROR";
  }
  return "INFO ";
}

std::string getFormattedTimestamp() {
  auto now = std::chrono::system_clock::now();
  auto inTime = std::chrono::system_clock::to_time_t(now);
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      now.time_since_epoch()) % 1000;

  std::tm tmBuf{};
#if defined(_WIN32)
  localtime_s(&tmBuf, &inTime);
#else
  localtime_r(&inTime, &tmBuf);
#endif

  std::ostringstream ss;
  ss << std::put_time(&tmBuf, "%Y-%m-%d %H:%M:%S") << "."
     << std::setfill(char(48)) << std::setw(3) << ms.count();
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

void logger::init(const std::filesystem::path& logPath) {
  std::lock_guard<std::mutex> lock(mutex_);
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

  logFile_ << "[" << getFormattedTimestamp() << "] ["
           << levelToString(level) << "] "
           << message << "\n";
  logFile_.flush();
}

void logger::debug(std::string_view message) {
  get().log(logLevel::Debug, message);
}

void logger::info(std::string_view message) {
  get().log(logLevel::Info, message);
}

void logger::warn(std::string_view message) {
  get().log(logLevel::Warn, message);
}

void logger::error(std::string_view message) {
  get().log(logLevel::Error, message);
}

std::filesystem::path logger::getLogPath() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return logPath_;
}

}  // namespace catalist
