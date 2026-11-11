#ifndef CATALIST_TESTS_SUPPORT_TEST_SUPPORT_HPP_
#define CATALIST_TESTS_SUPPORT_TEST_SUPPORT_HPP_

#include <filesystem>
#include <string>

#include "catalist/model/entry.hpp"
#include "catalist/platform/fileSystem.hpp"
#include "catalist/storage/database.hpp"
#include "support/fakes.hpp"

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
  [[nodiscard]] realFileSystem& fileSystem() { return fileSystem_; }

private:
  std::filesystem::path tempDir_;
  realFileSystem fileSystem_;
};

// Test data must spell dates correctly; a typo should fail loudly instead
// of silently becoming "no date".
isoDate requireDate(const std::string& text);

class entryBuilder {
public:
  entryBuilder& id(int value) { built_.id = value; return *this; }
  entryBuilder& malId(int value) { built_.malId = value; return *this; }
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

// The entry titled `title`; fails the test when there is none.
[[nodiscard]] entry findTitle(const database& db, mediaType type,
                              const std::string& title);

std::filesystem::path fixturePath(const std::string& relativePath);

}  // namespace catalist::test

#endif  // CATALIST_TESTS_SUPPORT_TEST_SUPPORT_HPP_
