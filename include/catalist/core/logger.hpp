#ifndef CATALIST_CORE_LOGGER_HPP_
#define CATALIST_CORE_LOGGER_HPP_

#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>

namespace catalist {

enum class LogLevel {
  Debug,
  Info,
  Warn,
  Error
};

class Logger {
 public:
  static Logger& get();

  void init(const std::filesystem::path& logPath);
  void log(LogLevel level, std::string_view message);

  static void debug(std::string_view message);
  static void info(std::string_view message);
  static void warn(std::string_view message);
  static void error(std::string_view message);

  [[nodiscard]] std::filesystem::path getLogPath() const;

 private:
  Logger();
  ~Logger();

  std::filesystem::path logPath_;
  std::ofstream logFile_;
  mutable std::mutex mutex_;
  bool initialized_ = false;

  void ensureOpen();
};

#define LOG_DEBUG(msg) ::catalist::Logger::debug(msg)
#define LOG_INFO(msg)  ::catalist::Logger::info(msg)
#define LOG_WARN(msg)  ::catalist::Logger::warn(msg)
#define LOG_ERROR(msg) ::catalist::Logger::error(msg)

}  // namespace catalist

#endif  // CATALIST_CORE_LOGGER_HPP_
