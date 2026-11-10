#include "support/testSupport.hpp"

#include <atomic>
#include <random>
#include <stdexcept>

#include <unistd.h>

namespace catalist::test {

namespace {

std::string randomSuffix() {
  static std::atomic<unsigned> counter{0};
  std::random_device device;
  return std::to_string(::getpid()) + "-" + std::to_string(counter++) + "-" +
         std::to_string(device());
}

}  // namespace

std::filesystem::path makeUniqueTempDir(const std::string& prefix) {
  auto base = std::filesystem::temp_directory_path();
  for (int attempt = 0; attempt < 16; ++attempt) {
    auto candidate = base / (prefix + randomSuffix());
    if (std::filesystem::create_directory(candidate))
      return candidate;
  }
  throw std::runtime_error("could not create a unique temp directory");
}

tempDirFixture::tempDirFixture()
    : tempDir_(makeUniqueTempDir("catalist-test-")) {}

tempDirFixture::~tempDirFixture() {
  std::error_code ignored;
  std::filesystem::remove_all(tempDir_, ignored);
}

isoDate requireDate(const std::string& text) {
  auto parsed = isoDate::parse(text);
  if (!parsed)
    throw std::invalid_argument("bad test date: " + text);
  return *parsed;
}

entryBuilder makeEntry() {
  return entryBuilder{};
}

fixedClock makeTestClock() {
  return fixedClock(iClock::timePoint(std::chrono::seconds(1792497600)));
}

entry findTitle(const database& db, mediaType type, const std::string& title) {
  for (const auto& e : db.getEntries(type)) {
    if (e.title == title)
      return e;
  }
  throw std::runtime_error("no entry titled " + title);
}

std::filesystem::path fixturePath(const std::string& relativePath) {
  return std::filesystem::path(CATALIST_FIXTURE_DIR) / relativePath;
}

}  // namespace catalist::test
