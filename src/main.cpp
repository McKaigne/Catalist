#include <clocale>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <string>
#include <unistd.h>

#include "catalist/cli.hpp"
#include "catalist/core/clock.hpp"
#include "catalist/core/logger.hpp"
#include "catalist/storage/database.hpp"
#include "catalist/platform/fileSystem.hpp"
#include "catalist/platform/processLauncher.hpp"
#include "catalist/tui/app.hpp"

namespace {

void safeWrite(const char* str) {
  if (!str) return;
  size_t len = std::strlen(str);
  while (len > 0) {
    ssize_t ret = write(STDERR_FILENO, str, len);
    if (ret <= 0) break;
    str += ret;
    len -= static_cast<size_t>(ret);
  }
}

void signalCrashHandler(int signum) {
  // Only async-signal-safe calls here: the crash may have happened while a
  // lock or the allocator was in use.
  safeWrite("\033[?1049l\033[?25h\033[0m\n");
  safeWrite("\n===============================================================\n");
  safeWrite("                      CATALIST CRASHED                         \n");
  safeWrite("===============================================================\n");
  safeWrite("Fatal OS signal received: ");
  switch (signum) {
    case SIGSEGV: safeWrite("SIGSEGV (Segmentation Fault)\n"); break;
    case SIGABRT: safeWrite("SIGABRT (Abort / Assertion Failure)\n"); break;
    case SIGFPE:  safeWrite("SIGFPE (Arithmetic Error)\n"); break;
    case SIGILL:  safeWrite("SIGILL (Illegal Instruction)\n"); break;
    case SIGBUS:  safeWrite("SIGBUS (Bus Error)\n"); break;
    default:      safeWrite("Signal\n"); break;
  }
  safeWrite("Log file written to ~/.config/catalist/catalist.log\n");
  safeWrite("===============================================================\n\n");
  std::_Exit(128 + signum);
}

void setupCrashHandlers() {
  std::signal(SIGSEGV, signalCrashHandler);
  std::signal(SIGABRT, signalCrashHandler);
  std::signal(SIGFPE,  signalCrashHandler);
  std::signal(SIGILL,  signalCrashHandler);
  std::signal(SIGBUS,  signalCrashHandler);

  std::set_terminate([]() {
    std::string err = "Unknown exception";
    try {
      auto ep = std::current_exception();
      if (ep) std::rethrow_exception(ep);
    } catch (const std::exception& e) {
      err = e.what();
    } catch (...) {
      err = "Non-standard exception";
    }

    LOG_ERROR("FATAL EXCEPTION: " + err);
    safeWrite("\033[?1049l\033[?25h\033[0m\n");
    std::cerr
        << "\n===============================================================\n"
        << "                      CATALIST CRASHED                         \n"
        << "===============================================================\n"
        << "An uncaught C++ exception caused Catalist to abort.\n\n"
        << "  Details: " << err << "\n\n"
        << "===============================================================\n\n";
    std::_Exit(1);
  });
}

}  // namespace

int main(int argc, char* argv[]) {
  std::setlocale(LC_ALL, "");
#if !defined(_WIN32)
  std::signal(SIGPIPE, SIG_IGN);
#endif

  setupCrashHandlers();

  catalist::realClock clock;
  catalist::realFileSystem fileSystem;
  catalist::realProcessLauncher launcher;
  const auto dataDir = catalist::database::getDefaultConfigDir();

  catalist::logger::get().init(dataDir / "catalist.log", fileSystem, clock);
  LOG_INFO("Catalist starting up...");

  catalist::database db(fileSystem, clock, dataDir);
  if (argc > 1) {
    return catalist::runCli({argv + 1, argv + argc}, db, fileSystem, std::cout,
                            std::cerr);
  }

  auto loaded = db.loadAll();
  if (!loaded.ok()) {
    std::cerr << "error: " << loaded.errorMessage() << "\n";
    return 1;
  }

  LOG_INFO("Launching interactive TUI screen.");
  catalist::runTui(db, clock, launcher);
  LOG_INFO("Catalist session exited cleanly.");

  return 0;
}
