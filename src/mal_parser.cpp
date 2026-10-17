#include "catalist/mal_parser.hpp"
#include <fstream>
#include <regex>

namespace catalist {

static std::string extractTag(const std::string& block, const std::string& tag) {
  std::regex re("<" + tag + ">(.*?)</" + tag + ">");
  std::smatch match;
  if (std::regex_search(block, match, re)) {
    std::string val = match[1].str();
    if (val.rfind("<![CDATA[", 0) == 0 && val.size() >= 12 && val.substr(val.size() - 3) == "]]>") {
      val = val.substr(9, val.size() - 12);
    }
    return val;
  }
  return "";
}

size_t MalParser::importXml(const std::string& filepath, Database& db, bool clean) {
  std::ifstream file(filepath);
  if (!file.is_open()) return 0;

  db.createBackupSnapshot();

  std::string content((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());

  size_t imported = 0;

  // Ingest Anime blocks
  std::regex animeRe("<anime>([\\s\\S]*?)</anime>");
  auto animeBegin = std::sregex_iterator(content.begin(), content.end(), animeRe);
  auto animeEnd = std::sregex_iterator();

  if (clean && animeBegin != animeEnd) {
    db.clear(MediaType::Anime);
  }

  for (auto it = animeBegin; it != animeEnd; ++it) {
    std::string block = it->str();
    Entry e;
    e.type = MediaType::Anime;

    std::string idStr = extractTag(block, "series_animedb_id");
    try { e.malId = idStr.empty() ? 0 : std::stoi(idStr); } catch (...) { e.malId = 0; }
    e.id = e.malId;

    e.title = extractTag(block, "series_title");
    if (e.title.empty()) continue;

    std::string scoreStr = extractTag(block, "my_score");
    try { e.score = scoreStr.empty() ? 0 : std::stoi(scoreStr); } catch (...) { e.score = 0; }

    std::string progressStr = extractTag(block, "my_watched_episodes");
    try { e.progress = progressStr.empty() ? 0 : std::stoi(progressStr); } catch (...) { e.progress = 0; }

    std::string totalStr = extractTag(block, "series_episodes");
    try { e.totalUnits = totalStr.empty() ? 0 : std::stoi(totalStr); } catch (...) { e.totalUnits = 0; }

    std::string rewatchStr = extractTag(block, "my_times_watched");
    try { e.rewatched = rewatchStr.empty() ? 0 : std::stoi(rewatchStr); } catch (...) { e.rewatched = 0; }

    std::string statusStr = extractTag(block, "my_status");
    e.status = stringToStatus(statusStr);

    e.startDate = extractTag(block, "my_start_date");
    e.finishDate = extractTag(block, "my_finish_date");
    e.notes = extractTag(block, "my_comments");

    db.addEntry(e);
    imported++;
  }

  // Ingest Manga blocks
  std::regex mangaRe("<manga>([\\s\\S]*?)</manga>");
  auto mangaBegin = std::sregex_iterator(content.begin(), content.end(), mangaRe);
  auto mangaEnd = std::sregex_iterator();

  if (clean && mangaBegin != mangaEnd) {
    db.clear(MediaType::Manga);
  }

  for (auto it = mangaBegin; it != mangaEnd; ++it) {
    std::string block = it->str();
    Entry e;
    e.type = MediaType::Manga;

    std::string idStr = extractTag(block, "manga_mangadb_id");
    try { e.malId = idStr.empty() ? 0 : std::stoi(idStr); } catch (...) { e.malId = 0; }
    e.id = e.malId;

    e.title = extractTag(block, "manga_title");
    if (e.title.empty()) continue;

    std::string scoreStr = extractTag(block, "my_score");
    try { e.score = scoreStr.empty() ? 0 : std::stoi(scoreStr); } catch (...) { e.score = 0; }

    std::string progressStr = extractTag(block, "my_read_chapters");
    try { e.progress = progressStr.empty() ? 0 : std::stoi(progressStr); } catch (...) { e.progress = 0; }

    std::string totalStr = extractTag(block, "manga_chapters");
    try { e.totalUnits = totalStr.empty() ? 0 : std::stoi(totalStr); } catch (...) { e.totalUnits = 0; }

    std::string volStr = extractTag(block, "my_read_volumes");
    try { e.volumes = volStr.empty() ? 0 : std::stoi(volStr); } catch (...) { e.volumes = 0; }

    std::string totVolStr = extractTag(block, "manga_volumes");
    try { e.totalVolumes = totVolStr.empty() ? 0 : std::stoi(totVolStr); } catch (...) { e.totalVolumes = 0; }

    std::string rereadStr = extractTag(block, "my_times_read");
    try { e.rewatched = rereadStr.empty() ? 0 : std::stoi(rereadStr); } catch (...) { e.rewatched = 0; }

    std::string statusStr = extractTag(block, "my_status");
    e.status = stringToStatus(statusStr);

    e.startDate = extractTag(block, "my_start_date");
    e.finishDate = extractTag(block, "my_finish_date");
    e.notes = extractTag(block, "my_comments");

    db.addEntry(e);
    imported++;
  }

  if (imported > 0) {
    db.saveAll();
  }

  return imported;
}

} // namespace catalist
