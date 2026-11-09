#include "support/testSupport.hpp"

#include <atomic>
#include <cstdlib>
#include <iostream>
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

bool goldenUpdateRequested() {
  const char* flag = std::getenv("CATALIST_UPDATE_GOLDEN");
  return flag != nullptr && std::string(flag) == "1";
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
    : tempDir_(makeUniqueTempDir("catalist-test-")), clock_(makeTestClock()) {}

tempDirFixture::~tempDirFixture() {
  std::error_code ignored;
  std::filesystem::remove_all(tempDir_, ignored);
}

streamCapture::streamCapture(std::ostream& target)
    : target_(target), buffer_(), original_(target.rdbuf(buffer_.rdbuf())) {}

streamCapture::~streamCapture() {
  target_.rdbuf(original_);
}

stdinFeed::stdinFeed(const std::string& content)
    : buffer_(content), original_(std::cin.rdbuf(buffer_.rdbuf())) {}

stdinFeed::~stdinFeed() {
  std::cin.rdbuf(original_);
  std::cin.clear();
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

std::filesystem::path fixturePath(const std::string& relativePath) {
  return std::filesystem::path(CATALIST_FIXTURE_DIR) / relativePath;
}

std::string readTextFile(const std::filesystem::path& path) {
  return realFileSystem().readFile(path).value_or("");
}

void writeTextFile(const std::filesystem::path& path,
                   const std::string& content) {
  if (!realFileSystem().writeFileAtomic(path, content))
    throw std::runtime_error("could not write test file " + path.string());
}

std::string loadGolden(const std::string& name, const std::string& actual) {
  auto path = fixturePath("golden/" + name);
  if (goldenUpdateRequested())
    writeTextFile(path, actual);
  return readTextFile(path);
}

}  // namespace catalist::test
