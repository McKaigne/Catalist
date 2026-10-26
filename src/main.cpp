#include "catalist/database.hpp"
#include "catalist/config.hpp"
#include "catalist/tui.hpp"
#include "catalist/cli.hpp"
#include <clocale>
#include <csignal>

int main(int argc, char* argv[]) {
  std::setlocale(LC_ALL, "");
#if !defined(_WIN32)
  std::signal(SIGPIPE, SIG_IGN);
#endif

  catalist::Database db;
  catalist::AppConfig config;

  auto configPath = catalist::Database::getConfigDir() / "catalist.kdl";
  config.load(configPath);

  if (!db.loadAll()) {
    db.saveAll();
  }

  if (argc > 1) {
    return catalist::CliDispatcher::dispatch(argc, argv, db, config);
  }

  catalist::AppTui app(db, config);
  app.run();

  return 0;
}
