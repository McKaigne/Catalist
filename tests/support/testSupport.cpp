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

isoDate requireDate(const std::string& text) {
  auto parsed = isoDate::parse(text);
  if (!parsed)
    throw std::invalid_argument("bad test date: " + text);
  return *parsed;
}

accountFakes::accountFakes(iFileSystem& fileSystem, iHttpClient& http,
                           const iClock& clock,
                           const std::filesystem::path& dataDir)
    : malLogin(fileSystem, http, launcher, authServer, clock,
               dataDir / "mal_auth.json"),
      anilistLogin(fileSystem, dataDir / "anilist_auth.json", "1234"),
      mal(http, malLogin, nullptr),
      anilist(http, anilistLogin, nullptr),
      accounts{malLogin, anilistLogin, mal, anilist} {}

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

}  // namespace catalist::test
