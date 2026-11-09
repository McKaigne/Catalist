#ifndef CATALIST_PLATFORM_FILE_SYSTEM_HPP_
#define CATALIST_PLATFORM_FILE_SYSTEM_HPP_

#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace catalist {

class iFileSystem {
public:
  using path = std::filesystem::path;

  virtual ~iFileSystem() = default;

  [[nodiscard]] virtual std::optional<std::string> readFile(
      const path& file) const = 0;
  // Writes to a sibling temp file and renames it over the target, so readers
  // never observe a half-written file.
  [[nodiscard]] virtual bool writeFileAtomic(const path& file,
                                             const std::string& content) = 0;
  [[nodiscard]] virtual bool appendFile(const path& file,
                                        const std::string& content) = 0;
  [[nodiscard]] virtual bool exists(const path& target) const = 0;
  virtual bool createDirectories(const path& directory) = 0;
  [[nodiscard]] virtual bool copyFile(const path& from, const path& to) = 0;
  virtual bool removeFile(const path& file) = 0;
  // Regular files directly inside the directory (no subdirectories).
  [[nodiscard]] virtual std::vector<path> listDirectory(
      const path& directory) const = 0;
  virtual bool restrictToOwner(const path& file) = 0;
};

class realFileSystem : public iFileSystem {
public:
  [[nodiscard]] std::optional<std::string> readFile(
      const path& file) const override;
  [[nodiscard]] bool writeFileAtomic(const path& file,
                                     const std::string& content) override;
  [[nodiscard]] bool appendFile(const path& file,
                                const std::string& content) override;
  [[nodiscard]] bool exists(const path& target) const override;
  bool createDirectories(const path& directory) override;
  [[nodiscard]] bool copyFile(const path& from, const path& to) override;
  bool removeFile(const path& file) override;
  [[nodiscard]] std::vector<path> listDirectory(
      const path& directory) const override;
  bool restrictToOwner(const path& file) override;
};

class memoryFileSystem : public iFileSystem {
public:
  [[nodiscard]] std::optional<std::string> readFile(
      const path& file) const override;
  [[nodiscard]] bool writeFileAtomic(const path& file,
                                     const std::string& content) override;
  [[nodiscard]] bool appendFile(const path& file,
                                const std::string& content) override;
  [[nodiscard]] bool exists(const path& target) const override;
  bool createDirectories(const path& directory) override;
  [[nodiscard]] bool copyFile(const path& from, const path& to) override;
  bool removeFile(const path& file) override;
  [[nodiscard]] std::vector<path> listDirectory(
      const path& directory) const override;
  bool restrictToOwner(const path& file) override;

  [[nodiscard]] bool isOwnerOnly(const path& file) const;
  void failWritesTo(const path& file) { failingWrites_.insert(key(file)); }

private:
  static std::string key(const path& target);
  std::map<std::string, std::string> files_;
  std::set<std::string> directories_;
  std::set<std::string> ownerOnly_;
  std::set<std::string> failingWrites_;
};

}  // namespace catalist

#endif  // CATALIST_PLATFORM_FILE_SYSTEM_HPP_
