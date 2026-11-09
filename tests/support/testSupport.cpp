#include "support/testSupport.hpp"

#include <atomic>
#include <cstdlib>
#include <fstream>
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
    : tempDir_(makeUniqueTempDir("catalist-test-")) {}

tempDirFixture::~tempDirFixture() {
  std::error_code ignored;
  std::filesystem::remove_all(tempDir_, ignored);
}

streamCapture::streamCapture(std::ostream& target)
    : target_(target), original_(target.rdbuf(buffer_.rdbuf())) {}

streamCapture::~streamCapture() {
  target_.rdbuf(original_);
}

stdinFeed::stdinFeed(const std::string& content)
    : buffer_(content), original_(std::cin.rdbuf(buffer_.rdbuf())) {}

stdinFeed::~stdinFeed() {
  std::cin.rdbuf(original_);
  std::cin.clear();
}

entryBuilder makeEntry() {
  return entryBuilder{};
}

std::filesystem::path fixturePath(const std::string& relativePath) {
  return std::filesystem::path(CATALIST_FIXTURE_DIR) / relativePath;
}

std::string readTextFile(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open())
    return "";
  return std::string((std::istreambuf_iterator<char>(file)),
                     std::istreambuf_iterator<char>());
}

void writeTextFile(const std::filesystem::path& path,
                   const std::string& content) {
  if (!path.parent_path().empty())
    std::filesystem::create_directories(path.parent_path());
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  file << content;
}

std::string loadGolden(const std::string& name, const std::string& actual) {
  auto path = fixturePath("golden/" + name);
  if (goldenUpdateRequested())
    writeTextFile(path, actual);
  return readTextFile(path);
}

}  // namespace catalist::test
