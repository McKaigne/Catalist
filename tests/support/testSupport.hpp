#ifndef CATALIST_TESTS_SUPPORT_TEST_SUPPORT_HPP_
#define CATALIST_TESTS_SUPPORT_TEST_SUPPORT_HPP_

#include <chrono>
#include <filesystem>
#include <optional>
#include <iosfwd>
#include <sstream>
#include <string>
#include <unordered_map>

#include "catalist/net/accounts.hpp"
#include "catalist/settings/settings.hpp"
#include "catalist/core/clock.hpp"
#include "catalist/platform/httpClient.hpp"
#include "catalist/storage/database.hpp"
#include "catalist/model/entry.hpp"
#include "catalist/platform/fileSystem.hpp"
#include "catalist/platform/processLauncher.hpp"

namespace catalist::test {

class noAuthServer : public iAuthCallbackServer {
public:
  std::optional<std::string> waitForCode(std::chrono::seconds) override {
    return std::nullopt;
  }
};

// Logged-out MyAnimeList and AniList services over the given files and HTTP
// client, with no request pacing.
struct accountFakes {
  accountFakes(iFileSystem& fileSystem, iHttpClient& http, const iClock& clock,
               const std::filesystem::path& dataDir);

  fakeProcessLauncher launcher;
  noAuthServer authServer;
  malAuth malLogin;
  anilistAuth anilistLogin;
  malListService mal;
  anilistListService anilist;
  listAccounts accounts;
};

std::filesystem::path makeUniqueTempDir(const std::string& prefix);

class tempDirFixture {
public:
  tempDirFixture();
  ~tempDirFixture();
  tempDirFixture(const tempDirFixture&) = delete;
  tempDirFixture& operator=(const tempDirFixture&) = delete;

  [[nodiscard]] const std::filesystem::path& tempDir() const {
    return tempDir_;
  }
  [[nodiscard]] realFileSystem& fileSystem() { return fileSystem_; }
  [[nodiscard]] fixedClock& clock() { return clock_; }

private:
  std::filesystem::path tempDir_;
  realFileSystem fileSystem_;
  fixedClock clock_;
};

class streamCapture {
public:
  explicit streamCapture(std::ostream& target);
  ~streamCapture();
  streamCapture(const streamCapture&) = delete;
  streamCapture& operator=(const streamCapture&) = delete;

  [[nodiscard]] std::string text() const { return buffer_.str(); }

private:
  // buffer_ must be declared before original_: the constructor hands
  // buffer_'s streambuf to the target while initialising original_.
  std::ostream& target_;
  std::ostringstream buffer_;
  std::streambuf* original_;
};

// Test data must spell dates correctly; a typo should fail loudly instead
// of silently becoming "no date".
isoDate requireDate(const std::string& text);

class entryBuilder {
public:
  entryBuilder& id(int value) { built_.id = value; return *this; }
  entryBuilder& malId(int value) { built_.malId = value; return *this; }
  entryBuilder& anilistId(int value) { built_.anilistId = value; return *this; }
  entryBuilder& title(std::string value) {
    built_.title = std::move(value);
    return *this;
  }
  entryBuilder& type(mediaType value) { built_.type = value; return *this; }
  entryBuilder& status(watchStatus value) {
    built_.status = value;
    return *this;
  }
  entryBuilder& score(int value) { built_.score = value; return *this; }
  entryBuilder& progress(int value) { built_.progress = value; return *this; }
  entryBuilder& totalUnits(int value) {
    built_.totalUnits = value;
    return *this;
  }
  entryBuilder& volumes(int value) { built_.volumes = value; return *this; }
  entryBuilder& totalVolumes(int value) {
    built_.totalVolumes = value;
    return *this;
  }
  entryBuilder& rewatched(int value) { built_.rewatched = value; return *this; }
  entryBuilder& startDate(const std::string& value) {
    built_.startDate = requireDate(value);
    return *this;
  }
  entryBuilder& finishDate(const std::string& value) {
    built_.finishDate = requireDate(value);
    return *this;
  }
  entryBuilder& notes(std::string value) {
    built_.notes = std::move(value);
    return *this;
  }
  entryBuilder& updatedAt(const std::string& value) {
    built_.updatedAt = requireDate(value);
    return *this;
  }

  [[nodiscard]] entry build() const { return built_; }
  operator entry() const { return built_; }

private:
  entry built_;
};

[[nodiscard]] entryBuilder makeEntry();

// 2026-10-20 12:00:00 UTC; the test main pins TZ=UTC so local dates agree.
[[nodiscard]] fixedClock makeTestClock();

std::filesystem::path fixturePath(const std::string& relativePath);
std::string readTextFile(const std::filesystem::path& path);
void writeTextFile(const std::filesystem::path& path,
                   const std::string& content);

}  // namespace catalist::test

#endif  // CATALIST_TESTS_SUPPORT_TEST_SUPPORT_HPP_
