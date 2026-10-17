#include "catalist/database.hpp"
#include "catalist/config.hpp"
#include "catalist/tui.hpp"
#include "catalist/malParser.hpp"

#include <iostream>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
  catalist::Database db;
  catalist::AppConfig config;

  auto configPath = catalist::Database::getConfigDir() / "config.toml";
  config.load(configPath);

  if (!db.loadAll()) {
    db.seedDefaults();
    db.saveAll();
  }

  if (argc > 1) {
    std::string command = argv[1];
    if (command == "--version" || command == "-v") {
      std::cout << "Catalist v0.2.0 (C++17 / FTXUI)\n";
      return 0;
    }
    if (command == "--help" || command == "-h") {
      std::cout << "Usage: catalist [command] [options]\n\n"
                << "Commands:\n"
                << "  (no args)                        Launch interactive unified dashboard\n"
                << "  import [--clean] <file.xml ...>  Ingest MyAnimeList XML export (optionally clean slate)\n"
                << "  --version                        Show version info\n"
                << "  --help                           Show this message\n";
      return 0;
    }
    if (command == "import" && argc >= 3) {
      bool clean = false;
      std::vector<std::string> xmlFiles;

      for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--clean" || arg == "--wipe") {
          clean = true;
        } else {
          xmlFiles.push_back(arg);
        }
      }

      for (const auto& file : xmlFiles) {
        std::cout << (clean ? "Clean-importing XML from: " : "Importing XML from: ") << file << "...\n";
        size_t count = catalist::MalParser::importXml(file, db, clean);
        std::cout << "Successfully ingested " << count << " records into ~/.config/catalist/.\n";
      }
      return 0;
    }
  }

  catalist::AppTui app(db, config);
  app.run();

  return 0;
}
