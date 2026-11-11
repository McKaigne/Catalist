#include "system.hpp"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iterator>
#include <thread>

namespace catalist {

std::optional<std::string> readFile(const std::filesystem::path& file) {
  // A directory opens as a stream on Linux but throws when read.
  std::error_code ec;
  if (!std::filesystem::is_regular_file(file, ec))
    return std::nullopt;
  std::ifstream stream(file, std::ios::binary);
  if (!stream.is_open())
    return std::nullopt;
  return std::string((std::istreambuf_iterator<char>(stream)),
                     std::istreambuf_iterator<char>());
}

bool writeFileAtomic(const std::filesystem::path& file,
                     const std::string& content) {
  std::error_code ec;
  if (!file.parent_path().empty())
    std::filesystem::create_directories(file.parent_path(), ec);
  std::filesystem::path tempFile = file.string() + ".tmp";
  bool written = false;
  {
    std::ofstream stream(tempFile, std::ios::binary | std::ios::trunc);
    stream << content;
    stream.flush();
    written = stream.is_open() && stream.good();
  }
  if (written)
    std::filesystem::rename(tempFile, file, ec);
  if (written && !ec)
    return true;
  std::filesystem::remove(tempFile, ec);
  return false;
}

std::optional<std::string> runProgram(const std::vector<std::string>& arguments,
                                      const std::string& input) {
  int toChild[2];
  int fromChild[2];
  if (pipe(toChild) != 0)
    return std::nullopt;
  if (pipe(fromChild) != 0) {
    close(toChild[0]);
    close(toChild[1]);
    return std::nullopt;
  }

  pid_t child = fork();
  if (child == 0) {
    // In the child: the pipes become standard input and output.
    dup2(toChild[0], STDIN_FILENO);
    dup2(fromChild[1], STDOUT_FILENO);
    int devNull = open("/dev/null", O_WRONLY);
    if (devNull >= 0)
      dup2(devNull, STDERR_FILENO);
    for (int end : {toChild[0], toChild[1], fromChild[0], fromChild[1]})
      close(end);
    std::vector<char*> argv;
    for (const auto& argument : arguments)
      argv.push_back(const_cast<char*>(argument.c_str()));
    argv.push_back(nullptr);
    execvp(argv[0], argv.data());
    _exit(127);
  }
  close(toChild[0]);
  close(fromChild[1]);
  if (child < 0) {
    close(toChild[1]);
    close(fromChild[0]);
    return std::nullopt;
  }

  // Feed the input from another thread, so a program that answers before it
  // has read everything cannot block on a full pipe.
  std::thread feeder([&input, end = toChild[1]] {
    size_t written = 0;
    while (written < input.size()) {
      ssize_t count =
          write(end, input.data() + written, input.size() - written);
      if (count <= 0)
        break;
      written += static_cast<size_t>(count);
    }
    close(end);
  });
  std::string output;
  std::array<char, 4096> buffer;
  ssize_t count = 0;
  while ((count = read(fromChild[0], buffer.data(), buffer.size())) > 0)
    output.append(buffer.data(), static_cast<size_t>(count));
  close(fromChild[0]);
  feeder.join();

  int status = 0;
  waitpid(child, &status, 0);
  bool succeeded = WIFEXITED(status) && WEXITSTATUS(status) == 0;
  return succeeded ? std::optional<std::string>(output) : std::nullopt;
}

bool hasProgram(const std::string& name) {
  const char* path = std::getenv("PATH");
  std::string directories = path ? path : "";
  size_t start = 0;
  while (start <= directories.size()) {
    size_t end = directories.find(':', start);
    if (end == std::string::npos)
      end = directories.size();
    std::filesystem::path candidate =
        std::filesystem::path(directories.substr(start, end - start)) / name;
    if (access(candidate.c_str(), X_OK) == 0)
      return true;
    start = end + 1;
  }
  return false;
}

std::optional<std::string> httpGet(const std::string& url) {
  // -f turns an HTTP error into a failed exit, -m bounds the whole request so
  // nothing waits longer than that.
  return runProgram({"curl", "-sfL", "-m", "8", "-A", "catalist", url});
}

isoDate today() {
  std::time_t seconds =
      std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  std::tm local{};
  localtime_r(&seconds, &local);
  return isoDate::fromLocalTm(local);
}

std::optional<cellSize> terminalCellSize() {
  winsize window{};
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &window) != 0 || window.ws_col == 0 ||
      window.ws_row == 0 || window.ws_xpixel == 0 || window.ws_ypixel == 0)
    return std::nullopt;
  return cellSize{window.ws_xpixel / window.ws_col,
                  window.ws_ypixel / window.ws_row};
}

}  // namespace catalist
