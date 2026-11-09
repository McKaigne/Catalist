#include <clocale>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <unistd.h>

#include "catalist/cli.hpp"
#include "catalist/config.hpp"
#include "catalist/core/logger.hpp"
#include "catalist/database.hpp"
#include "catalist/tui.hpp"

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
  // Pure async-signal-safe recovery to prevent deadlocks
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

  auto logPath = catalist::database::getConfigDir() / "catalist.log";
  catalist::logger::get().init(logPath);
  LOG_INFO("Catalist starting up...");

  catalist::database db;
  catalist::appConfig config;

  auto configPath = catalist::database::getConfigDir() / "catalist.kdl";
  config.load(configPath);
  LOG_INFO("Loaded workspace config from: " + configPath.string());

  if (!db.loadAll()) {
    LOG_WARN("No catalog databases found on disk, initializing defaults.");
    db.saveAll();
  }

  if (argc > 1) {
    LOG_INFO("Dispatching CLI command: " + std::string(argv[1]));
    return catalist::cliDispatcher::dispatch(argc, argv, db, config);
  }

  LOG_INFO("Launching interactive TUI screen.");
  catalist::appTui app(db, config);
  app.run();
  LOG_INFO("Catalist session exited cleanly.");

  return 0;
}
