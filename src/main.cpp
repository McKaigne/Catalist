#include <clocale>
#include <csignal>
#include <iostream>

#include "catalogue.hpp"
#include "cli.hpp"
#include "system.hpp"
#include "tui/app.hpp"

int main(int argc, char* argv[]) {
  std::setlocale(LC_ALL, "");
  // `catalist export anime | head` must end quietly, not die on the signal.
  std::signal(SIGPIPE, SIG_IGN);

  catalist::realFileSystem fileSystem;
  catalist::catalogue db(fileSystem, catalist::catalogue::getDefaultConfigDir());
  if (argc > 1)
    return catalist::runCli({argv + 1, argv + argc}, db, fileSystem, std::cout,
                            std::cerr);

  auto loaded = db.loadAll();
  if (!loaded.ok()) {
    std::cerr << "error: " << loaded.errorMessage() << "\n";
    return 1;
  }

  catalist::realClock clock;
  catalist::curlHttpClient http;
  catalist::runTui(db, clock, http);
  return 0;
}
