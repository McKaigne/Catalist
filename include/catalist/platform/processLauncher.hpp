#ifndef CATALIST_PLATFORM_PROCESS_LAUNCHER_HPP_
#define CATALIST_PLATFORM_PROCESS_LAUNCHER_HPP_

#include <string>
#include <vector>

#include "catalist/core/result.hpp"

namespace catalist {

struct processOutput {
  int exitCode = 0;
  std::string standardOutput;
};

// Programs are always given as argv vectors, never as shell strings, so no
// caller can build a command line out of untrusted text.
class iProcessLauncher {
public:
  virtual ~iProcessLauncher() = default;

  [[nodiscard]] virtual result<processOutput> capture(
      const std::vector<std::string>& argv) = 0;
  [[nodiscard]] virtual bool feed(const std::vector<std::string>& argv,
                                  const std::string& standardInput) = 0;
  virtual bool spawnDetached(const std::vector<std::string>& argv) = 0;
};

class realProcessLauncher : public iProcessLauncher {
public:
  [[nodiscard]] result<processOutput> capture(
      const std::vector<std::string>& argv) override;
  [[nodiscard]] bool feed(const std::vector<std::string>& argv,
                          const std::string& standardInput) override;
  bool spawnDetached(const std::vector<std::string>& argv) override;
};

class fakeProcessLauncher : public iProcessLauncher {
public:
  struct call {
    std::vector<std::string> argv;
    std::string standardInput;
    bool detached = false;
  };

  [[nodiscard]] result<processOutput> capture(
      const std::vector<std::string>& argv) override;
  [[nodiscard]] bool feed(const std::vector<std::string>& argv,
                          const std::string& standardInput) override;
  bool spawnDetached(const std::vector<std::string>& argv) override;

  [[nodiscard]] const std::vector<call>& calls() const { return calls_; }

private:
  std::vector<call> calls_;
};

[[nodiscard]] std::vector<std::string> browserOpenCommand(
    const std::string& url);

}  // namespace catalist

#endif  // CATALIST_PLATFORM_PROCESS_LAUNCHER_HPP_
