#include "catalist/platform/processLauncher.hpp"

#include <sys/wait.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <memory>

#include "catalist/core/shell.hpp"

namespace catalist {

namespace {

struct pipeCloser {
  void operator()(FILE* pipe) const {
    if (pipe)
      pclose(pipe);
  }
};

int exitCodeFromStatus(int status) {
  if (status == -1 || !WIFEXITED(status))
    return -1;
  return WEXITSTATUS(status);
}

std::string quietCommand(const std::vector<std::string>& argv) {
  return shell::joinEscaped(argv) + " 2>/dev/null";
}

}  // namespace

result<processOutput> realProcessLauncher::capture(
    const std::vector<std::string>& argv) {
  if (argv.empty())
    return result<processOutput>::failure("empty argv");
  FILE* rawPipe = popen(quietCommand(argv).c_str(), "r");
  if (!rawPipe)
    return result<processOutput>::failure("failed to spawn " + argv[0]);

  processOutput output;
  std::array<char, 512> buffer;
  while (fgets(buffer.data(), static_cast<int>(buffer.size()), rawPipe))
    output.standardOutput += buffer.data();
  output.exitCode = exitCodeFromStatus(pclose(rawPipe));
  return result<processOutput>::success(std::move(output));
}

bool realProcessLauncher::feed(const std::vector<std::string>& argv,
                               const std::string& standardInput) {
  if (argv.empty())
    return false;
  FILE* rawPipe = popen(quietCommand(argv).c_str(), "w");
  if (!rawPipe)
    return false;
  fwrite(standardInput.data(), 1, standardInput.size(), rawPipe);
  return exitCodeFromStatus(pclose(rawPipe)) == 0;
}

bool realProcessLauncher::spawnDetached(const std::vector<std::string>& argv) {
  if (argv.empty())
    return false;
  std::string command = shell::joinEscaped(argv) + " >/dev/null 2>&1 &";
  return std::system(command.c_str()) == 0;
}

std::vector<std::string> browserOpenCommand(const std::string& url) {
#if defined(__APPLE__)
  return {"open", url};
#else
  return {"xdg-open", url};
#endif
}

void fakeProcessLauncher::setCaptureOutput(const std::string& program,
                                           processOutput output) {
  captureOutputs_[program] = std::move(output);
}

void fakeProcessLauncher::setFeedSucceeds(const std::string& program,
                                          bool succeeds) {
  feedResults_[program] = succeeds;
}

result<processOutput> fakeProcessLauncher::capture(
    const std::vector<std::string>& argv) {
  calls_.push_back({argv, "", false});
  auto scripted = argv.empty() ? captureOutputs_.end()
                               : captureOutputs_.find(argv[0]);
  if (scripted == captureOutputs_.end())
    return result<processOutput>::failure("fakeProcessLauncher: no output");
  return result<processOutput>::success(scripted->second);
}

bool fakeProcessLauncher::feed(const std::vector<std::string>& argv,
                               const std::string& standardInput) {
  calls_.push_back({argv, standardInput, false});
  auto scripted = argv.empty() ? feedResults_.end() : feedResults_.find(argv[0]);
  return scripted != feedResults_.end() && scripted->second;
}

bool fakeProcessLauncher::spawnDetached(const std::vector<std::string>& argv) {
  calls_.push_back({argv, "", true});
  return true;
}

}  // namespace catalist
