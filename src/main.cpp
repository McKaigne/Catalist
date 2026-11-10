#include <clocale>
#include <csignal>
#include <iostream>

#include "catalist/cli.hpp"
#include "catalist/core/clock.hpp"
#include "catalist/platform/fileSystem.hpp"
#include "catalist/platform/httpClient.hpp"
#include "catalist/storage/database.hpp"
#include "catalist/tui/app.hpp"

int main(int argc, char* argv[]) {
  std::setlocale(LC_ALL, "");
  // `catalist export anime | head` must end quietly, not die on the signal.
  std::signal(SIGPIPE, SIG_IGN);

  catalist::realFileSystem fileSystem;
  catalist::database db(fileSystem, catalist::database::getDefaultConfigDir());
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
