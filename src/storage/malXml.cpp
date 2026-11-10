#include "catalist/storage/malXml.hpp"

#include <algorithm>

namespace catalist::malXml {

namespace {

std::string decodeXmlEntities(std::string text) {
  auto replaceAll = [&](const std::string& from, const std::string& to) {
    size_t pos = 0;
    while ((pos = text.find(from, pos)) != std::string::npos) {
      text.replace(pos, from.length(), to);
      pos += to.length();
    }
  };
  replaceAll("&lt;", "<");
  replaceAll("&gt;", ">");
  replaceAll("&quot;", "\"");
  replaceAll("&#039;", "'");
  replaceAll("&apos;", "'");
  // Last, so "&amp;lt;" stays the text "&lt;".
  replaceAll("&amp;", "&");
  return text;
}

// The trimmed text of the first <tag> in the block; empty when absent.
std::string tagText(const std::string& block, const std::string& tag) {
  std::string openTag = "<" + tag + ">";
  std::string closeTag = "</" + tag + ">";
  size_t start = block.find(openTag);
  if (start == std::string::npos)
    return "";
  start += openTag.length();
  size_t end = block.find(closeTag, start);
  if (end == std::string::npos)
    return "";

  std::string text = block.substr(start, end - start);
  const std::string cdataOpen = "<![CDATA[";
  size_t cdataStart = text.find(cdataOpen);
  size_t cdataEnd = text.rfind("]]>");
  if (cdataStart != std::string::npos && cdataEnd != std::string::npos &&
      cdataEnd >= cdataStart + cdataOpen.length()) {
    cdataStart += cdataOpen.length();
    text = text.substr(cdataStart, cdataEnd - cdataStart);
    // write() splits a literal "]]>" over two sections.
    const std::string seam = "]]]]><![CDATA[>";
    for (size_t at = text.find(seam); at != std::string::npos;
         at = text.find(seam, at + 3))
      text.replace(at, seam.length(), "]]>");
    return text;
  }

  size_t first = text.find_first_not_of(" \t\r\n");
  if (first == std::string::npos)
    return "";
  size_t last = text.find_last_not_of(" \t\r\n");
  return decodeXmlEntities(text.substr(first, last - first + 1));
}

// A count is plain digits; anything else (or an absurd length) reads as 0.
int tagNumber(const std::string& block, const std::string& tag) {
  std::string digits = tagText(block, tag);
  if (digits.length() > 8)
    return 0;
  int number = 0;
  for (char c : digits) {
    if (c < '0' || c > '9')
      return 0;
    number = number * 10 + (c - '0');
  }
  return number;
}

struct blockTags {
  mediaType type;
  const char* block;
  const char* id;
  const char* title;
  const char* total;
  const char* progress;
  const char* repeats;
};

const blockTags kAnimeTags = {mediaType::kAnime,     "anime",
                              "series_animedb_id",   "series_title",
                              "series_episodes",     "my_watched_episodes",
                              "my_times_watched"};
const blockTags kMangaTags = {mediaType::kManga,  "manga",
                              "manga_mangadb_id", "manga_title",
                              "manga_chapters",   "my_read_chapters",
                              "my_times_read"};

void parseBlocks(const std::string& xml, const blockTags& tags,
                 std::vector<entry>& found) {
  std::string openTag = std::string("<") + tags.block + ">";
  std::string closeTag = std::string("</") + tags.block + ">";
  size_t at = 0;
  while ((at = xml.find(openTag, at)) != std::string::npos) {
    size_t end = xml.find(closeTag, at);
    if (end == std::string::npos)
      break;
    std::string block = xml.substr(at, end - at);
    at = end + closeTag.length();

    entry item;
    item.type = tags.type;
    item.title = tagText(block, tags.title);
    if (item.title.empty())
      continue;
    item.malId = tagNumber(block, tags.id);
    item.totalUnits = tagNumber(block, tags.total);
    item.progress = tagNumber(block, tags.progress);
    item.rewatched = tagNumber(block, tags.repeats);
    // Only manga blocks carry volumes; for anime both read as 0.
    item.totalVolumes = tagNumber(block, "manga_volumes");
    item.volumes = tagNumber(block, "my_read_volumes");
    item.score = std::min(tagNumber(block, "my_score"), 10);
    item.status = parseStatus(tagText(block, "my_status"))
                      .value_or(watchStatus::kPlanToWatch);
    // MyAnimeList writes 0000-00-00 for "no date", which parses as absent.
    item.startDate = isoDate::parse(tagText(block, "my_start_date"));
    item.finishDate = isoDate::parse(tagText(block, "my_finish_date"));
    item.notes = tagText(block, "my_comments");
    found.push_back(item);
  }
}

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

std::vector<entry> parse(const std::string& xml) {
  std::vector<entry> found;
  parseBlocks(xml, kAnimeTags, found);
  parseBlocks(xml, kMangaTags, found);
  return found;
}

exported write(const std::vector<entry>& entries, mediaType type) {
  exported result;
  result.xml =
      "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<myanimelist>\n"
      "\t<myinfo>\n\t\t<user_export_type>" +
      std::string(type == mediaType::kAnime ? "1" : "2") +
      "</user_export_type>\n\t</myinfo>\n";
  for (const auto& item : entries) {
    if (item.type != type)
      continue;
    if (item.malId <= 0) {
      ++result.skipped;
      continue;
    }
    result.xml += entryBlock(item);
    ++result.written;
  }
  result.xml += "</myanimelist>\n";
  return result;
}

}  // namespace catalist::malXml
