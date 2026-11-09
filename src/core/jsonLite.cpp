#include "catalist/core/jsonLite.hpp"

#include <cctype>
#include <iomanip>
#include <sstream>

namespace catalist::jsonLite {

namespace {

std::optional<size_t> findValueStart(std::string_view block,
                                     std::string_view key,
                                     std::string_view separator) {
  std::string compact = "\"" + std::string(key) + "\":" + std::string(separator);
  std::string spaced = "\"" + std::string(key) + "\": " + std::string(separator);
  size_t pos = block.find(compact);
  if (pos != std::string_view::npos)
    return pos + compact.size();
  pos = block.find(spaced);
  if (pos != std::string_view::npos)
    return pos + spaced.size();
  return std::nullopt;
}

bool isUnescapedQuote(std::string_view text, size_t index) {
  return text[index] == '"' && (index == 0 || text[index - 1] != '\\');
}

bool isNumberLeadIn(char c) {
  return c == ' ' || c == '\t' || c == '"';
}

bool isNumberBody(char c) {
  return std::isdigit(static_cast<unsigned char>(c)) || c == '.';
}

std::optional<char> unescapedChar(char escaped) {
  switch (escaped) {
    case '"':
      return '"';
    case '\\':
      return '\\';
    case '/':
      return '/';
    case 'n':
      return '\n';
    case 't':
      return '\t';
    default:
      return std::nullopt;
  }
}

}  // namespace

std::optional<std::string> extractJsonString(std::string_view block,
                                             std::string_view key) {
  auto start = findValueStart(block, key, "\"");
  if (!start)
    return std::nullopt;
  size_t end = *start;
  while (end < block.size() && !isUnescapedQuote(block, end))
    ++end;
  return std::string(block.substr(*start, end - *start));
}

std::optional<double> extractJsonNumber(std::string_view block,
                                        std::string_view key) {
  auto start = findValueStart(block, key, "");
  if (!start)
    return std::nullopt;
  size_t begin = *start;
  while (begin < block.size() && isNumberLeadIn(block[begin]))
    ++begin;
  size_t end = begin;
  while (end < block.size() && isNumberBody(block[end]))
    ++end;
  if (end == begin)
    return std::nullopt;
  try {
    return std::stod(std::string(block.substr(begin, end - begin)));
  } catch (...) {
    return std::nullopt;
  }
}

namespace {

struct objectScan {
  size_t end = 0;
  bool complete = false;
};

objectScan scanObject(std::string_view json, size_t objectStart) {
  int depth = 0;
  bool inString = false;
  for (size_t i = objectStart; i < json.size(); ++i) {
    if (isUnescapedQuote(json, i)) {
      inString = !inString;
      continue;
    }
    if (inString)
      continue;
    if (json[i] == '{')
      ++depth;
    if (json[i] == '}' && --depth == 0)
      return {i, true};
  }
  return {json.size(), false};
}

}  // namespace

std::vector<std::string> splitArrayObjects(std::string_view json,
                                           std::string_view arrayKey) {
  std::vector<std::string> objects;
  size_t keyPos = json.find("\"" + std::string(arrayKey) + "\"");
  if (keyPos == std::string_view::npos)
    return objects;
  size_t cursor = json.find('[', keyPos);
  if (cursor == std::string_view::npos)
    return objects;

  for (++cursor; cursor < json.size(); ++cursor) {
    if (json[cursor] == ']')
      break;
    if (json[cursor] != '{')
      continue;
    auto scan = scanObject(json, cursor);
    if (!scan.complete)
      break;
    objects.emplace_back(json.substr(cursor, scan.end - cursor + 1));
    cursor = scan.end;
  }
  return objects;
}

std::string unescapeJsonString(std::string_view text) {
  std::string decoded;
  decoded.reserve(text.size());
  for (size_t i = 0; i < text.size(); ++i) {
    bool hasEscape = text[i] == '\\' && i + 1 < text.size();
    auto replacement = hasEscape ? unescapedChar(text[i + 1]) : std::nullopt;
    if (!replacement) {
      decoded += text[i];
      continue;
    }
    decoded += *replacement;
    ++i;
  }
  return decoded;
}

std::string escapeJsonString(std::string_view text) {
  std::ostringstream escaped;
  for (char c : text) {
    switch (c) {
      case '"':
        escaped << "\\\"";
        break;
      case '\\':
        escaped << "\\\\";
        break;
      case '\b':
        escaped << "\\b";
        break;
      case '\f':
        escaped << "\\f";
        break;
      case '\n':
        escaped << "\\n";
        break;
      case '\r':
        escaped << "\\r";
        break;
      case '\t':
        escaped << "\\t";
        break;
      default:
        if ('\x00' <= c && c <= '\x1f') {
          escaped << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                  << static_cast<int>(c) << std::dec;
        } else {
          escaped << c;
        }
    }
  }
  return escaped.str();
}

}  // namespace catalist::jsonLite
