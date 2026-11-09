#ifndef CATALIST_CORE_LOGGER_HPP_
#define CATALIST_CORE_LOGGER_HPP_

#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>

#include "catalist/core/clock.hpp"
#include "catalist/platform/fileSystem.hpp"

namespace catalist {

enum class logLevel {
  kDebug,
  kInfo,
  kWarn,
  kError
};

// Process-wide log sink. main() wires it to a file through init(); until
// then messages are dropped, so library code and tests never create log
// files on their own.
class logger {
 public:
  static logger& get();

  void init(const std::filesystem::path& logPath, iFileSystem& fileSystem,
            const iClock& clock);
  void log(logLevel level, std::string_view message);

  static void debug(std::string_view message);
  static void info(std::string_view message);
  static void warn(std::string_view message);
  static void error(std::string_view message);

  [[nodiscard]] std::filesystem::path getLogPath() const;

 private:
  logger() = default;

  // Guards all fields: the TUI logs from its background sync thread too.
  mutable std::mutex mutex_;
  std::filesystem::path logPath_;
  iFileSystem* fileSystem_ = nullptr;
  const iClock* clock_ = nullptr;
};

#define LOG_DEBUG(msg) ::catalist::logger::debug(msg)
#define LOG_INFO(msg)  ::catalist::logger::info(msg)
#define LOG_WARN(msg)  ::catalist::logger::warn(msg)
#define LOG_ERROR(msg) ::catalist::logger::error(msg)

}  // namespace catalist

#endif  // CATALIST_CORE_LOGGER_HPP_
