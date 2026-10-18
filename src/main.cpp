#include "catalist/database.hpp"
#include "catalist/config.hpp"
#include "catalist/tui.hpp"
#include "catalist/cli.hpp"

int main(int argc, char* argv[]) {
  catalist::Database db;
  catalist::AppConfig config;

  auto configPath = catalist::Database::getConfigDir() / "config.toml";
  config.load(configPath);

  if (!db.loadAll()) {
    db.seedDefaults();
    db.saveAll();
  }

  // If CLI arguments are provided, route to the headless CLI dispatcher
  if (argc > 1) {
    return catalist::CliDispatcher::dispatch(argc, argv, db, config);
  }

  // Default interactive mode: unified TUI dashboard
  catalist::AppTui app(db, config);
  app.run();

  return 0;
}
