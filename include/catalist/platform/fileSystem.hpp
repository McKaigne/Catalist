#ifndef CATALIST_PLATFORM_FILE_SYSTEM_HPP_
#define CATALIST_PLATFORM_FILE_SYSTEM_HPP_

#include <filesystem>
#include <optional>
#include <string>

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

#endif  // CATALIST_PLATFORM_FILE_SYSTEM_HPP_
