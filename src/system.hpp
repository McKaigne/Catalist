#ifndef CATALIST_SYSTEM_HPP_
#define CATALIST_SYSTEM_HPP_

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "date.hpp"

namespace catalist {

// The two things Catalist does with files, behind an interface so the tests
// can run on an in-memory fake.
class iFileSystem {
public:
  using path = std::filesystem::path;

  virtual ~iFileSystem() = default;

  // Absent when the file does not exist or cannot be read.
  [[nodiscard]] virtual std::optional<std::string> readFile(
      const path& file) const = 0;
  // Creates the parent directory if needed, writes to a sibling temp file and
  // renames it over the target, so a crash never leaves a half-written file.
  [[nodiscard]] virtual bool writeFileAtomic(const path& file,
                                             const std::string& content) = 0;
};

class realFileSystem : public iFileSystem {
public:
  [[nodiscard]] std::optional<std::string> readFile(
      const path& file) const override;
  [[nodiscard]] bool writeFileAtomic(const path& file,
                                     const std::string& content) override;
};

}  // namespace catalist

namespace catalist {

// Where "today" comes from, so the tests can pin the date.
class iClock {
public:
  using timePoint = std::chrono::system_clock::time_point;

  virtual ~iClock() = default;
  [[nodiscard]] virtual timePoint now() const = 0;
};

class realClock : public iClock {
public:
  [[nodiscard]] timePoint now() const override;
};

[[nodiscard]] isoDate localDate(const iClock& clock);

}  // namespace catalist

namespace catalist {

// The only way Catalist reaches the network: one GET for the title search.
class iHttpClient {
public:
  virtual ~iHttpClient() = default;
  // The reply body, or nothing when the request failed for any reason: no
  // connection, a timeout or an HTTP error status.
  [[nodiscard]] virtual std::optional<std::string> get(
      const std::string& url) = 0;
};

// Runs the curl program, so Catalist itself links no network library.
class curlHttpClient : public iHttpClient {
public:
  [[nodiscard]] std::optional<std::string> get(const std::string& url) override;
};

}  // namespace catalist

namespace catalist::shell {

// POSIX single-quote escaping: the result is always one shell word, whatever
// the input contains.
[[nodiscard]] std::string escapeShellArg(std::string_view arg);

}  // namespace catalist::shell

#endif  // CATALIST_SYSTEM_HPP_
