#include "catalist/malParser.hpp"
#include <fstream>
#include <string>

namespace catalist {

static std::string decodeXmlEntities(std::string text) {
  auto replaceAll = [&](const std::string& from, const std::string& to) {
    size_t pos = 0;
    while ((pos = text.find(from, pos)) != std::string::npos) {
      text.replace(pos, from.length(), to);
      pos += to.length();
    }
  };
  replaceAll("&amp;", "&");
  replaceAll("&lt;", "<");
  replaceAll("&gt;", ">");
  replaceAll("&quot;", "\"");
  replaceAll("&#039;", "'");
  replaceAll("&apos;", "'");
  return text;
}

static std::string extractTag(const std::string& block, const std::string& tag) {
  std::string openTag = "<" + tag + ">";
  std::string closeTag = "</" + tag + ">";
  size_t start = block.find(openTag);
  if (start == std::string::npos) return "";
  start += openTag.length();
  size_t end = block.find(closeTag, start);
  if (end == std::string::npos) return "";

  std::string val = block.substr(start, end - start);
  size_t cdataStart = val.find("<![CDATA[");
  if (cdataStart != std::string::npos) {
    size_t cdataEnd = val.find("]]>", cdataStart + 9);
    if (cdataEnd != std::string::npos) {
      val = val.substr(cdataStart + 9, cdataEnd - (cdataStart + 9));
    }
  }

  size_t first = val.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return "";
  size_t last = val.find_last_not_of(" \t\r\n");
  val = val.substr(first, last - first + 1);

  return decodeXmlEntities(val);
}

size_t MalParser::importXml(const std::string& filepath, Database& db, bool clean) {
  std::ifstream file(filepath);
  if (!file.is_open()) return 0;

  db.createBackupSnapshot();

  std::string content((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());

  size_t imported = 0;

  if (clean && content.find("<anime>") != std::string::npos) {
    db.clear(MediaType::Anime);
  }

  size_t aPos = 0;
  while ((aPos = content.find("<anime>", aPos)) != std::string::npos) {
    size_t aEnd = content.find("</anime>", aPos);
    if (aEnd == std::string::npos) break;
    std::string block = content.substr(aPos, aEnd - aPos + 8);
    aPos = aEnd + 8;

    Entry e;
    e.type = MediaType::Anime;

    std::string idStr = extractTag(block, "series_animedb_id");
    try { e.malId = idStr.empty() ? 0 : std::stoi(idStr); } catch (...) { e.malId = 0; }
    if (e.malId > 0) {
      e.id = e.malId;
      e.setRemoteId("mal", std::to_string(e.malId));
    }

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

  if (clean && content.find("<manga>") != std::string::npos) {
    db.clear(MediaType::Manga);
  }

  size_t mPos = 0;
  while ((mPos = content.find("<manga>", mPos)) != std::string::npos) {
    size_t mEnd = content.find("</manga>", mPos);
    if (mEnd == std::string::npos) break;
    std::string block = content.substr(mPos, mEnd - mPos + 8);
    mPos = mEnd + 8;

    Entry e;
    e.type = MediaType::Manga;

    std::string idStr = extractTag(block, "manga_mangadb_id");
    try { e.malId = idStr.empty() ? 0 : std::stoi(idStr); } catch (...) { e.malId = 0; }
    if (e.malId > 0) {
      e.id = e.malId;
      e.setRemoteId("mal", std::to_string(e.malId));
    }

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
