#ifndef CATALIST_TESTS_SUPPORT_FAKES_HPP_
#define CATALIST_TESTS_SUPPORT_FAKES_HPP_

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "catalist/core/clock.hpp"
#include "catalist/platform/fileSystem.hpp"
#include "catalist/platform/httpClient.hpp"

// In-memory stand-ins for the file system, the network and the clock.
namespace catalist::test {

class memoryFileSystem : public iFileSystem {
public:
  [[nodiscard]] std::optional<std::string> readFile(
      const path& file) const override {
    auto found = files_.find(key(file));
    if (found == files_.end())
      return std::nullopt;
    return found->second;
  }
  [[nodiscard]] bool writeFileAtomic(const path& file,
                                     const std::string& content) override {
    if (failingWrites_.count(key(file)))
      return false;
    files_[key(file)] = content;
    return true;
  }

  void failWritesTo(const path& file) { failingWrites_.insert(key(file)); }
  [[nodiscard]] size_t fileCount() const { return files_.size(); }

private:
  static std::string key(const path& target) {
    return target.lexically_normal().generic_string();
  }
  std::map<std::string, std::string> files_;
  std::set<std::string> failingWrites_;
};

// Answers a GET whose URL contains a registered fragment with its body, and
// fails every other request, as if offline. Records every URL asked for.
class mockHttpClient : public iHttpClient {
public:
  void respond(const std::string& urlFragment, const std::string& body) {
    bodies_[urlFragment] = body;
  }
  [[nodiscard]] std::optional<std::string> get(
      const std::string& url) override {
    requested.push_back(url);
    for (const auto& [fragment, body] : bodies_) {
      if (url.find(fragment) != std::string::npos)
        return body;
    }
    return std::nullopt;
  }

  std::vector<std::string> requested;

private:
  std::map<std::string, std::string> bodies_;
};

class fixedClock : public iClock {
public:
  explicit fixedClock(timePoint frozenAt) : frozenAt_(frozenAt) {}
  [[nodiscard]] timePoint now() const override { return frozenAt_; }

private:
  timePoint frozenAt_;
};

}  // namespace catalist::test

#endif  // CATALIST_TESTS_SUPPORT_FAKES_HPP_
