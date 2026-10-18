#include "catalist/remoteSearch.hpp"

#include <array>
#include <memory>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <cstdlib>

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

std::vector<RemoteSearchResult> RemoteSearchEngine::query(MediaType type,
                                                         const std::string& searchQuery,
                                                         std::string& outErr,
                                                         size_t limit) {
  std::vector<RemoteSearchResult> results;
  if (searchQuery.empty()) return results;

  std::string endpoint = (type == MediaType::Anime) ? "anime" : "manga";
  std::string url = "https://api.jikan.moe/v4/" + endpoint + "?q=" + urlEncode(searchQuery) + "&limit=" + std::to_string(limit);
  std::string cmd = "curl -s -m 5 \"" + url + "\" 2>/dev/null";

  std::string json = execCommand(cmd);
  if (json.empty()) {
    outErr = "Failed to contact Jikan API (network unreachable or timeout).";
    return results;
  }

  size_t dataPos = json.find("\"data\":[");
  if (dataPos == std::string::npos) {
    dataPos = json.find("\"data\": [");
    if (dataPos == std::string::npos) {
      outErr = "No records returned from upstream.";
      return results;
    }
    dataPos += 1;
  }

  size_t cur = dataPos + 8;
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
      if (fromDate.size() >= 4) {
        r.year = fromDate.substr(0, 4);
      }
    }

    results.push_back(r);
  }

  return results;
}

} // namespace catalist
