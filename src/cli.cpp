#include "catalist/cli.hpp"

#include <ostream>

#include "catalist/core/version.hpp"
#include "catalist/storage/malXml.hpp"

namespace catalist {

namespace {

constexpr const char* kUsage =
    "Usage:\n"
    "  catalist                         open the tracker\n"
    "  catalist import <file.xml>...    add the titles of MyAnimeList XML "
    "exports\n"
    "  catalist export <anime|manga>    print that list as MyAnimeList XML\n"
    "  catalist --help | --version\n";

int usageError(std::ostream& err, const std::string& message) {
  err << "error: " << message << "\n" << kUsage;
  return 2;
}

void printHelp(std::ostream& out, const database& db) {
  out << "Catalist " << kVersionString
      << " - an anime and manga tracker for the terminal.\n\n"
      << kUsage
      << "\nImporting never overwrites what you already track: a title that "
         "is in the\ncatalogue keeps its score, progress and dates. "
         "MyAnimeList and AniList\nboth read the exported file on their "
         "import pages.\n\nThe catalogue is kept in "
      << db.getConfigPath().string() << ".\n";
}

int importFiles(const std::vector<std::string>& files, database& db,
                const iFileSystem& fileSystem, std::ostream& out,
                std::ostream& err) {
  // Everything is read before anything is added, so a bad file leaves the
  // catalogue untouched.
  std::vector<std::vector<entry>> lists;
  for (const auto& file : files) {
    auto content = fileSystem.readFile(file);
    if (!content) {
      err << "error: cannot read " << file << "\n";
      return 1;
    }
    // MyAnimeList hands out its exports gzipped.
    if (content->rfind("\x1f\x8b", 0) == 0) {
      err << "error: " << file << " is compressed; unpack it first (gunzip "
          << file << ")\n";
      return 1;
    }
    lists.push_back(malXml::parse(*content));
    if (lists.back().empty()) {
      err << "error: no anime or manga found in " << file
          << " (is it a MyAnimeList XML export?)\n";
      return 1;
    }
  }

  for (size_t i = 0; i < files.size(); ++i) {
    size_t added = 0;
    for (const auto& title : lists[i]) {
      size_t before = db.getEntries(title.type).size();
      db.addEntry(title);
      added += db.getEntries(title.type).size() - before;
    }
    out << files[i] << ": " << lists[i].size() << " titles, " << added
        << " added, " << lists[i].size() - added
        << " already in the catalogue\n";
  }

  if (!db.saveAll()) {
    err << "error: could not save the catalogue to "
        << db.getConfigPath().string() << "\n";
    return 1;
  }
  return 0;
}

int exportList(mediaType type, const database& db, std::ostream& out,
               std::ostream& err) {
  auto list = malXml::write(db.getEntries(type), type);
  out << list.xml;
  if (list.skipped > 0)
    err << "note: " << list.skipped
        << " titles without a MyAnimeList id were left out.\n";
  return 0;
}

}  // namespace

int runCli(const std::vector<std::string>& args, database& db,
           const iFileSystem& fileSystem, std::ostream& out,
           std::ostream& err) {
  const std::string& command = args.at(0);
  if (command == "--version" || command == "-v") {
    out << "Catalist v" << kVersionString << "\n";
    return 0;
  }
  if (command == "--help" || command == "-h") {
    printHelp(out, db);
    return 0;
  }
  if (command != "import" && command != "export")
    return usageError(err, "unknown command '" + command + "'");

  if (command == "import" && args.size() < 2)
    return usageError(err, "import needs at least one file");
  bool anime = args.size() == 2 && args[1] == "anime";
  bool manga = args.size() == 2 && args[1] == "manga";
  if (command == "export" && !anime && !manga)
    return usageError(err, "export needs one of: anime, manga");

  auto loaded = db.loadAll();
  if (!loaded.ok()) {
    err << "error: " << loaded.errorMessage() << "\n";
    return 1;
  }
  if (command == "export")
    return exportList(anime ? mediaType::kAnime : mediaType::kManga, db, out,
                      err);
  return importFiles({args.begin() + 1, args.end()}, db, fileSystem, out, err);
}

}  // namespace catalist
