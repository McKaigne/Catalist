#include "catalist/database.hpp"
#include "catalist/config.hpp"
#include "catalist/tui.hpp"
#include "catalist/cli.hpp"
#include <clocale>

int main(int argc, char* argv[]) {
  // Initialize native UTF-8 locale for terminal PUA and Powerline glyph support
  std::setlocale(LC_ALL, "");

  catalist::Database db;
  catalist::AppConfig config;

  auto configPath = catalist::Database::getConfigDir() / "config.toml";
  config.load(configPath);

  if (!db.loadAll()) {
    // db.seedDefaults();
    db.saveAll();
  }

  // Headless CLI routing
  if (argc > 1) {
    return catalist::CliDispatcher::dispatch(argc, argv, db, config);
  }

  // Interactive TUI dashboard
  catalist::AppTui app(db, config);
  app.run();

  return 0;
}
