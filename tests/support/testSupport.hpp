#ifndef CATALIST_TESTS_SUPPORT_TEST_SUPPORT_HPP_
#define CATALIST_TESTS_SUPPORT_TEST_SUPPORT_HPP_

#include <filesystem>
#include <iosfwd>
#include <sstream>
#include <string>
#include <unordered_map>

#include "catalist/entry.hpp"

namespace catalist::test {

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

private:
  std::filesystem::path tempDir_;
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

class stdinFeed {
public:
  explicit stdinFeed(const std::string& content);
  ~stdinFeed();
  stdinFeed(const stdinFeed&) = delete;
  stdinFeed& operator=(const stdinFeed&) = delete;

private:
  // Same ordering constraint as streamCapture.
  std::istringstream buffer_;
  std::streambuf* original_;
};

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
  entryBuilder& startDate(std::string value) {
    built_.startDate = std::move(value);
    return *this;
  }
  entryBuilder& finishDate(std::string value) {
    built_.finishDate = std::move(value);
    return *this;
  }
  entryBuilder& notes(std::string value) {
    built_.notes = std::move(value);
    return *this;
  }
  entryBuilder& updatedAt(std::string value) {
    built_.updatedAt = std::move(value);
    return *this;
  }
  entryBuilder& dirty(bool value) { built_.dirty = value; return *this; }
  entryBuilder& remoteId(const std::string& provider, const std::string& value) {
    built_.remoteIds[provider] = value;
    return *this;
  }

  [[nodiscard]] entry build() const { return built_; }
  operator entry() const { return built_; }

private:
  entry built_;
};

[[nodiscard]] entryBuilder makeEntry();

std::filesystem::path fixturePath(const std::string& relativePath);
std::string readTextFile(const std::filesystem::path& path);
void writeTextFile(const std::filesystem::path& path,
                   const std::string& content);

// Golden files are regenerated only when CATALIST_UPDATE_GOLDEN=1 so a normal
// run can never silently rewrite the expected output.
std::string loadGolden(const std::string& name, const std::string& actual);

}  // namespace catalist::test

#endif  // CATALIST_TESTS_SUPPORT_TEST_SUPPORT_HPP_
