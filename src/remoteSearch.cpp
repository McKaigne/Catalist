#include "catalist/remoteSearch.hpp"

#include <array>
#include <memory>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <filesystem>

namespace catalist {

struct PipeDeleter {
  void operator()(FILE* f) const {
    if (f) pclose(f);
  }
};

static std::string execCommand(const std::string& cmd) {
  std::array<char, 256> buffer;
  std::string result;
  std::unique_ptr<FILE, PipeDeleter> pipe(popen(cmd.c_str(), "r"));
  if (!pipe) return "";
  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr) {
    result += buffer.data();
  }
  return result;
}

std::string RemoteSearchEngine::urlEncode(const std::string& value) {
  std::ostringstream escaped;
  escaped.fill('0');
  escaped << std::hex;

  for (char c : value) {
    if (isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~') {
      escaped << c;
    } else {
      escaped << '%' << std::setw(2) << static_cast<int>(static_cast<unsigned char>(c));
    }
  }
  return escaped.str();
}

static std::string unescapeJsonString(const std::string& str) {
  std::string res;
  for (size_t i = 0; i < str.size(); ++i) {
    if (str[i] == '\\' && i + 1 < str.size()) {
      char next = str[i + 1];
      if (next == '"') { res += '"'; ++i; continue; }
      if (next == '\\') { res += '\\'; ++i; continue; }
      if (next == '/') { res += '/'; ++i; continue; }
      if (next == 'n') { res += '\n'; ++i; continue; }
      if (next == 't') { res += '\t'; ++i; continue; }
    }
    res += str[i];
  }
  return res;
}

static std::string extractJsonString(const std::string& block, const std::string& key) {
  std::string pattern = "\"" + key + "\":\"";
  size_t pos = block.find(pattern);
  if (pos == std::string::npos) {
    pattern = "\"" + key + "\": \"";
    pos = block.find(pattern);
    if (pos == std::string::npos) return "";
  }
  size_t start = pos + pattern.size();
  size_t end = start;
  while (end < block.size()) {
    if (block[end] == '"' && (end == 0 || block[end - 1] != '\\')) break;
    end++;
  }
  return (end > start) ? block.substr(start, end - start) : "";
}

static double extractJsonNumber(const std::string& block, const std::string& key) {
  std::string pattern = "\"" + key + "\":";
  size_t pos = block.find(pattern);
  if (pos == std::string::npos) {
    pattern = "\"" + key + "\": ";
    pos = block.find(pattern);
    if (pos == std::string::npos) return 0.0;
  }
  size_t start = pos + pattern.size();
  while (start < block.size() && (block[start] == ' ' || block[start] == '\t')) start++;
  if (start < block.size() && block[start] == 'n') return 0.0;
  size_t end = start;
  while (end < block.size() && (std::isdigit(block[end]) || block[end] == '.')) end++;
  if (end > start) {
    try { return std::stod(block.substr(start, end - start)); } catch (...) {}
  }
  return 0.0;
}

static std::vector<RemoteSearchResult> queryAniList(MediaType type, const std::string& searchQuery, size_t limit) {
  std::vector<RemoteSearchResult> results;

  std::filesystem::path tempPayload = std::filesystem::temp_directory_path() / ("catalist_anilist_" + std::to_string(std::rand()) + ".json");
  std::ofstream out(tempPayload);
  if (!out.is_open()) return results;

  std::string queryType = (type == MediaType::Anime) ? "ANIME" : "MANGA";

  out << "{\"query\":\"query($s:String,$t:MediaType){Page(perPage:" << limit
      << "){media(search:$s,type:$t){id idMal title{romaji english} episodes chapters status averageScore startDate{year}}}}\",\"variables\":{\"s\":\"";
  for (char c : searchQuery) {
    if (c == '"') out << "\\\"";
    else if (c == '\\') out << "\\\\";
    else out << c;
  }
  out << "\",\"t\":\"" << queryType << "\"}}";
  out.close();

  std::string cmd = "curl -s -L -m 8 -X POST \"https://graphql.anilist.co\" -H \"Content-Type: application/json\" -H \"User-Agent: Mozilla/5.0\" -d @\"" + tempPayload.string() + "\" 2>/dev/null";
  std::string json = execCommand(cmd);
  std::filesystem::remove(tempPayload);

  if (json.empty() || json.find("\"media\":[") == std::string::npos) {
    return results;
  }

  size_t cur = json.find("\"media\":[") + 9;
  while (cur < json.size() && results.size() < limit) {
    size_t objStart = json.find('{', cur);
    if (objStart == std::string::npos) break;

    int depth = 0;
    size_t objEnd = std::string::npos;
    bool inQuote = false;
    for (size_t i = objStart; i < json.size(); ++i) {
      if (json[i] == '"' && (i == 0 || json[i - 1] != '\\')) {
        inQuote = !inQuote;
      } else if (!inQuote) {
        if (json[i] == '{') depth++;
        else if (json[i] == '}') {
          depth--;
          if (depth == 0) {
            objEnd = i;
            break;
          }
        }
      }
    }

    if (objEnd == std::string::npos) break;
    std::string block = json.substr(objStart, objEnd - objStart + 1);
    cur = objEnd + 1;

    RemoteSearchResult r;
    r.type = type;

    r.malId = static_cast<int>(extractJsonNumber(block, "idMal"));
    int alId = static_cast<int>(extractJsonNumber(block, "id"));
    if (r.malId <= 0 && alId <= 0) continue;

    r.title = unescapeJsonString(extractJsonString(block, "romaji"));
    if (r.title.empty()) {
      r.title = unescapeJsonString(extractJsonString(block, "english"));
    }
    if (r.title.empty()) continue;

    r.titleEnglish = unescapeJsonString(extractJsonString(block, "english"));
    std::string unitKey = (type == MediaType::Anime) ? "episodes" : "chapters";
    r.totalUnits = static_cast<int>(extractJsonNumber(block, unitKey));

    double avg100 = extractJsonNumber(block, "averageScore");
    r.score = (avg100 > 0.0) ? (avg100 / 10.0) : 0.0;

    r.status = extractJsonString(block, "status");
    int yr = static_cast<int>(extractJsonNumber(block, "year"));
    if (yr > 1900) r.year = std::to_string(yr);

    results.push_back(r);
  }

  return results;
}

static std::vector<RemoteSearchResult> queryJikanFallback(MediaType type, const std::string& searchQuery, size_t limit) {
  std::vector<RemoteSearchResult> results;

  std::string endpoint = (type == MediaType::Anime) ? "anime" : "manga";
  std::string url = "https://api.jikan.moe/v4/" + endpoint + "?q=" + RemoteSearchEngine::urlEncode(searchQuery) + "&limit=" + std::to_string(limit);
  std::string cmd = "curl -s -L -m 8 -A \"Mozilla/5.0 (Windows NT 10.0; Win64; x64)\" \"" + url + "\" 2>/dev/null";

  std::string json = execCommand(cmd);
  if (json.empty() || json.find("\"data\":[") == std::string::npos) return results;

  size_t cur = json.find("\"data\":[") + 8;
  while (cur < json.size() && results.size() < limit) {
    size_t objStart = json.find('{', cur);
    if (objStart == std::string::npos) break;

    int depth = 0;
    size_t objEnd = std::string::npos;
    bool inQuote = false;
    for (size_t i = objStart; i < json.size(); ++i) {
      if (json[i] == '"' && (i == 0 || json[i - 1] != '\\')) {
        inQuote = !inQuote;
      } else if (!inQuote) {
        if (json[i] == '{') depth++;
        else if (json[i] == '}') {
          depth--;
          if (depth == 0) { objEnd = i; break; }
        }
      }
    }

    if (objEnd == std::string::npos) break;
    std::string block = json.substr(objStart, objEnd - objStart + 1);
    cur = objEnd + 1;

    RemoteSearchResult r;
    r.type = type;
    r.malId = static_cast<int>(extractJsonNumber(block, "mal_id"));
    if (r.malId <= 0) continue;

    r.title = unescapeJsonString(extractJsonString(block, "title"));
    if (r.title.empty()) continue;

    r.titleEnglish = unescapeJsonString(extractJsonString(block, "title_english"));
    std::string unitKey = (type == MediaType::Anime) ? "episodes" : "chapters";
    r.totalUnits = static_cast<int>(extractJsonNumber(block, unitKey));
    r.score = extractJsonNumber(block, "score");
    r.status = extractJsonString(block, "status");

    int yr = static_cast<int>(extractJsonNumber(block, "year"));
    if (yr > 1900) {
      r.year = std::to_string(yr);
    } else {
      std::string fromDate = extractJsonString(block, "from");
      if (fromDate.size() >= 4) r.year = fromDate.substr(0, 4);
    }

    results.push_back(r);
  }

  return results;
}

std::vector<RemoteSearchResult> RemoteSearchEngine::query(MediaType type,
                                                         const std::string& searchQuery,
                                                         std::string& outErr,
                                                         size_t limit) {
  if (searchQuery.empty()) return {};

  auto results = queryAniList(type, searchQuery, limit);
  if (!results.empty()) return results;

  results = queryJikanFallback(type, searchQuery, limit);
  if (!results.empty()) return results;

  outErr = "Failed to contact remote providers (AniList and Jikan). Check your internet connection.";
  return {};
}

} // namespace catalist
