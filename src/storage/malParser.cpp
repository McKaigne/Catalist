#include "catalist/storage/malParser.hpp"

#include <string>

#include "catalist/core/logger.hpp"

namespace catalist {

static watchStatus importedStatus(const std::string& field,
                                  const std::string& title) {
  auto status = parseStatus(field);
  if (!status)
    LOG_WARN("Unknown MAL status '" + field + "' for '" + title +
             "'; imported as Plan to Watch.");
  return status.value_or(watchStatus::kPlanToWatch);
}

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

static std::string extractTag(const std::string& block,
                              const std::string& tag) {
  std::string openTag = "<" + tag + ">";
  std::string closeTag = "</" + tag + ">";
  size_t start = block.find(openTag);
  if (start == std::string::npos)
    return "";
  start += openTag.length();
  size_t end = block.find(closeTag, start);
  if (end == std::string::npos)
    return "";

  std::string val = block.substr(start, end - start);
  size_t cdataStart = val.find("<![CDATA[");
  if (cdataStart != std::string::npos) {
    size_t cdataEnd = val.find("]]>", cdataStart + 9);
    if (cdataEnd != std::string::npos) {
      val = val.substr(cdataStart + 9, cdataEnd - (cdataStart + 9));
    }
  }

  size_t first = val.find_first_not_of(" \t\r\n");
  if (first == std::string::npos)
    return "";
  size_t last = val.find_last_not_of(" \t\r\n");
  val = val.substr(first, last - first + 1);

  return decodeXmlEntities(val);
}

size_t malParser::importXml(const iFileSystem& fileSystem,
                            const std::string& filepath, database& db,
                            bool clean) {
  auto fileContent = fileSystem.readFile(filepath);
  if (!fileContent)
    return 0;

  db.createBackupSnapshot();
  const std::string& content = *fileContent;

  size_t imported = 0;

  if (clean && content.find("<anime>") != std::string::npos) {
    db.clear(mediaType::kAnime);
  }

  size_t aPos = 0;
  while ((aPos = content.find("<anime>", aPos)) != std::string::npos) {
    size_t aEnd = content.find("</anime>", aPos);
    if (aEnd == std::string::npos)
      break;
    std::string block = content.substr(aPos, aEnd - aPos + 8);
    aPos = aEnd + 8;

    entry e;
    e.type = mediaType::kAnime;

    std::string idStr = extractTag(block, "series_animedb_id");
    try {
      e.malId = idStr.empty() ? 0 : std::stoi(idStr);
    } catch (...) {
      e.malId = 0;
    }
    if (e.malId > 0)
      e.id = e.malId;

    e.title = extractTag(block, "series_title");
    if (e.title.empty())
      continue;

    std::string scoreStr = extractTag(block, "my_score");
    try {
      e.score = scoreStr.empty() ? 0 : std::stoi(scoreStr);
    } catch (...) {
      e.score = 0;
    }

    std::string progressStr = extractTag(block, "my_watched_episodes");
    try {
      e.progress = progressStr.empty() ? 0 : std::stoi(progressStr);
    } catch (...) {
      e.progress = 0;
    }

    std::string totalStr = extractTag(block, "series_episodes");
    try {
      e.totalUnits = totalStr.empty() ? 0 : std::stoi(totalStr);
    } catch (...) {
      e.totalUnits = 0;
    }

    std::string rewatchStr = extractTag(block, "my_times_watched");
    try {
      e.rewatched = rewatchStr.empty() ? 0 : std::stoi(rewatchStr);
    } catch (...) {
      e.rewatched = 0;
    }

    std::string statusStr = extractTag(block, "my_status");
    e.status = importedStatus(statusStr, e.title);

    e.startDate = isoDate::parse(extractTag(block, "my_start_date"));
    e.finishDate = isoDate::parse(extractTag(block, "my_finish_date"));
    e.notes = extractTag(block, "my_comments");

    db.addEntry(e);
    imported++;
  }

  if (clean && content.find("<manga>") != std::string::npos) {
    db.clear(mediaType::kManga);
  }

  size_t mPos = 0;
  while ((mPos = content.find("<manga>", mPos)) != std::string::npos) {
    size_t mEnd = content.find("</manga>", mPos);
    if (mEnd == std::string::npos)
      break;
    std::string block = content.substr(mPos, mEnd - mPos + 8);
    mPos = mEnd + 8;

    entry e;
    e.type = mediaType::kManga;

    std::string idStr = extractTag(block, "manga_mangadb_id");
    try {
      e.malId = idStr.empty() ? 0 : std::stoi(idStr);
    } catch (...) {
      e.malId = 0;
    }
    if (e.malId > 0)
      e.id = e.malId;

    e.title = extractTag(block, "manga_title");
    if (e.title.empty())
      continue;

    std::string scoreStr = extractTag(block, "my_score");
    try {
      e.score = scoreStr.empty() ? 0 : std::stoi(scoreStr);
    } catch (...) {
      e.score = 0;
    }

    std::string progressStr = extractTag(block, "my_read_chapters");
    try {
      e.progress = progressStr.empty() ? 0 : std::stoi(progressStr);
    } catch (...) {
      e.progress = 0;
    }

    std::string totalStr = extractTag(block, "manga_chapters");
    try {
      e.totalUnits = totalStr.empty() ? 0 : std::stoi(totalStr);
    } catch (...) {
      e.totalUnits = 0;
    }

    std::string volStr = extractTag(block, "my_read_volumes");
    try {
      e.volumes = volStr.empty() ? 0 : std::stoi(volStr);
    } catch (...) {
      e.volumes = 0;
    }

    std::string totVolStr = extractTag(block, "manga_volumes");
    try {
      e.totalVolumes = totVolStr.empty() ? 0 : std::stoi(totVolStr);
    } catch (...) {
      e.totalVolumes = 0;
    }

    std::string rereadStr = extractTag(block, "my_times_read");
    try {
      e.rewatched = rereadStr.empty() ? 0 : std::stoi(rereadStr);
    } catch (...) {
      e.rewatched = 0;
    }

    std::string statusStr = extractTag(block, "my_status");
    e.status = importedStatus(statusStr, e.title);

    e.startDate = isoDate::parse(extractTag(block, "my_start_date"));
    e.finishDate = isoDate::parse(extractTag(block, "my_finish_date"));
    e.notes = extractTag(block, "my_comments");

    db.addEntry(e);
    imported++;
  }

  if (imported > 0) {
    db.saveAll();
  }

  return imported;
}

namespace {

// CDATA cannot contain "]]>", so it is split across two sections.
std::string cdata(const std::string& text) {
  std::string escaped;
  size_t start = 0;
  for (size_t found = text.find("]]>"); found != std::string::npos;
       found = text.find("]]>", start)) {
    escaped += text.substr(start, found - start) + "]]]]><![CDATA[>";
    start = found + 3;
  }
  return "<![CDATA[" + escaped + text.substr(start) + "]]>";
}

std::string xmlTag(const std::string& name, const std::string& value) {
  return "\t\t<" + name + ">" + value + "</" + name + ">\n";
}

std::string xmlStatus(const entry& item) {
  return statusToString(item.status, item.type);
}

std::string animeBlock(const entry& item) {
  std::string block = "\t<anime>\n";
  block += xmlTag("series_animedb_id", std::to_string(item.malId));
  block += xmlTag("series_title", cdata(item.title));
  block += xmlTag("series_episodes", std::to_string(item.totalUnits));
  block += xmlTag("my_watched_episodes", std::to_string(item.progress));
  block += xmlTag("my_times_watched", std::to_string(item.rewatched));
  return block;
}

std::string mangaBlock(const entry& item) {
  std::string block = "\t<manga>\n";
  block += xmlTag("manga_mangadb_id", std::to_string(item.malId));
  block += xmlTag("manga_title", cdata(item.title));
  block += xmlTag("manga_volumes", std::to_string(item.totalVolumes));
  block += xmlTag("manga_chapters", std::to_string(item.totalUnits));
  block += xmlTag("my_read_volumes", std::to_string(item.volumes));
  block += xmlTag("my_read_chapters", std::to_string(item.progress));
  block += xmlTag("my_times_read", std::to_string(item.rewatched));
  return block;
}

std::string entryBlock(const entry& item) {
  bool anime = item.type == mediaType::kAnime;
  std::string block = anime ? animeBlock(item) : mangaBlock(item);
  if (item.startDate)
    block += xmlTag("my_start_date", item.startDate->toString());
  if (item.finishDate)
    block += xmlTag("my_finish_date", item.finishDate->toString());
  block += xmlTag("my_score", std::to_string(item.score));
  block += xmlTag("my_status", xmlStatus(item));
  block += xmlTag("my_comments", cdata(item.notes));
  block += xmlTag("update_on_import", "1");
  return block + (anime ? "\t</anime>\n" : "\t</manga>\n");
}

}  // namespace

malXmlExport malParser::exportXml(const std::vector<entry>& entries,
                                  mediaType type) {
  malXmlExport exported;
  exported.xml =
      "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<myanimelist>\n"
      "\t<myinfo>\n\t\t<user_export_type>" +
      std::string(type == mediaType::kAnime ? "1" : "2") +
      "</user_export_type>\n\t</myinfo>\n";
  for (const auto& item : entries) {
    if (item.type != type)
      continue;
    if (item.malId <= 0) {
      ++exported.skipped;
      continue;
    }
    exported.xml += entryBlock(item);
    ++exported.written;
  }
  exported.xml += "</myanimelist>\n";
  return exported;
}

}  // namespace catalist
