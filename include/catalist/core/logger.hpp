#ifndef CATALIST_CORE_LOGGER_HPP_
#define CATALIST_CORE_LOGGER_HPP_

#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>

namespace catalist {

enum class logLevel {
  Debug,
  Info,
  Warn,
  Error
};

class logger {
 public:
  static logger& get();

  void init(const std::filesystem::path& logPath);
  void log(logLevel level, std::string_view message);

  static void debug(std::string_view message);
  static void info(std::string_view message);
  static void warn(std::string_view message);
  static void error(std::string_view message);

  [[nodiscard]] std::filesystem::path getLogPath() const;

 private:
  logger();
  ~logger();

  std::filesystem::path logPath_;
  std::ofstream logFile_;
  mutable std::mutex mutex_;
  bool initialized_ = false;

  void ensureOpen();
};

#define LOG_DEBUG(msg) ::catalist::logger::debug(msg)
#define LOG_INFO(msg)  ::catalist::logger::info(msg)
#define LOG_WARN(msg)  ::catalist::logger::warn(msg)
#define LOG_ERROR(msg) ::catalist::logger::error(msg)

}  // namespace catalist

#endif  // CATALIST_CORE_LOGGER_HPP_
