#include <clocale>
#include <csignal>
#include <iostream>

#include "catalogue.hpp"
#include "cli.hpp"
#include "tui/app.hpp"

int main(int argc, char* argv[]) {
  std::setlocale(LC_ALL, "");
  // `catalist export anime | head` must end quietly, not die on the signal.
  std::signal(SIGPIPE, SIG_IGN);

  catalist::catalogue db(catalist::catalogue::getDefaultConfigDir());
  if (argc > 1)
    return catalist::runCli({argv + 1, argv + argc}, db, std::cout, std::cerr);

  auto loaded = db.loadAll();
  if (!loaded.ok()) {
    std::cerr << "error: " << loaded.errorMessage() << "\n";
    return 1;
  }

  catalist::runTui(db);
  return 0;
}
