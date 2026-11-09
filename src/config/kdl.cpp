#include "catalist/config/kdl.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>

namespace catalist::kdl {

std::string value::asString() const {
  if (valueType == type::kString)
    return stringVal;
  if (valueType == type::kNumber) {
    if (numberVal == static_cast<long long>(numberVal)) {
      return std::to_string(static_cast<long long>(numberVal));
    }
    return std::to_string(numberVal);
  }
  if (valueType == type::kBoolean)
    return boolVal ? "true" : "false";
  return "";
}

int value::asInt(int defaultVal) const {
  if (valueType == type::kNumber)
    return static_cast<int>(numberVal);
  if (valueType == type::kString) {
    try {
      return std::stoi(stringVal);
    } catch (...) {
    }
  }
  return defaultVal;
}

double value::asDouble(double defaultVal) const {
  if (valueType == type::kNumber)
    return numberVal;
  if (valueType == type::kString) {
    try {
      return std::stod(stringVal);
    } catch (...) {
    }
  }
  return defaultVal;
}

bool value::asBool(bool defaultVal) const {
  if (valueType == type::kBoolean)
    return boolVal;
  if (valueType == type::kString) {
    std::string s = stringVal;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    if (s == "true" || s == "1" || s == "yes")
      return true;
    if (s == "false" || s == "0" || s == "no")
      return false;
  }
  return defaultVal;
}

bool node::hasProp(const std::string& key) const {
  return props.find(key) != props.end();
}

value node::getProp(const std::string& key, const value& defaultVal) const {
  auto it = props.find(key);
  if (it != props.end())
    return it->second;
  return defaultVal;
}

std::string node::getPropString(const std::string& key,
                                const std::string& defaultVal) const {
  auto it = props.find(key);
  if (it != props.end())
    return it->second.asString();
  return defaultVal;
}

int node::getPropInt(const std::string& key, int defaultVal) const {
  auto it = props.find(key);
  if (it != props.end())
    return it->second.asInt(defaultVal);
  return defaultVal;
}

bool node::getPropBool(const std::string& key, bool defaultVal) const {
  auto it = props.find(key);
  if (it != props.end())
    return it->second.asBool(defaultVal);
  return defaultVal;
}

value node::getArg(size_t index, const value& defaultVal) const {
  if (index < args.size())
    return args[index];
  return defaultVal;
}

std::string node::getArgString(size_t index,
                               const std::string& defaultVal) const {
  if (index < args.size())
    return args[index].asString();
  return defaultVal;
}

std::vector<node> node::getChildren(const std::string& childName) const {
  std::vector<node> res;
  for (const auto& child : children) {
    if (child.name == childName)
      res.push_back(child);
  }
  return res;
}

std::optional<node> node::getChild(const std::string& childName) const {
  for (const auto& child : children) {
    if (child.name == childName)
      return child;
  }
  return std::nullopt;
}

std::vector<node> document::getNodes(const std::string& name) const {
  std::vector<node> res;
  for (const auto& n : rootNodes) {
    if (n.name == name)
      res.push_back(n);
  }
  return res;
}

std::optional<node> document::getNode(const std::string& name) const {
  for (const auto& n : rootNodes) {
    if (n.name == name)
      return n;
  }
  return std::nullopt;
}

class tokenizer {
public:
  explicit tokenizer(std::string src) : src_(std::move(src)), pos_(0) {}

  enum class tokenType {
    kIdent,
    kString,
    kNumber,
    kEquals,
    kLBrace,
    kRBrace,
    kSemicolon,
    kNewline,
    kEof
  };

  struct token {
    tokenType type = tokenType::kEof;
    std::string text;
    double numberVal = 0.0;
  };

  token next() {
    skipWhitespaceAndComments();
    if (pos_ >= src_.size())
      return {tokenType::kEof, "", 0.0};

    char c = src_[pos_];

    if (c == '\n' || c == '\r') {
      while (pos_ < src_.size() && (src_[pos_] == '\n' || src_[pos_] == '\r'))
        pos_++;
      return {tokenType::kNewline, "\n", 0.0};
    }
    if (c == ';') {
      pos_++;
      return {tokenType::kSemicolon, ";", 0.0};
    }
    if (c == '=') {
      pos_++;
      return {tokenType::kEquals, "=", 0.0};
    }
    if (c == '{') {
      pos_++;
      return {tokenType::kLBrace, "{", 0.0};
    }
    if (c == '}') {
      pos_++;
      return {tokenType::kRBrace, "}", 0.0};
    }

    if (c == '"')
      return parseString();

    if (std::isdigit(static_cast<unsigned char>(c)) ||
        (c == '-' && pos_ + 1 < src_.size() &&
         std::isdigit(static_cast<unsigned char>(src_[pos_ + 1])))) {
      return parseNumber();
    }

    return parseIdent();
  }

private:
  std::string src_;
  size_t pos_;

  void skipWhitespaceAndComments() {
    while (pos_ < src_.size()) {
      char c = src_[pos_];
      if (c == ' ' || c == '\t') {
        pos_++;
        continue;
      }
      if (c == '/' && pos_ + 1 < src_.size()) {
        if (src_[pos_ + 1] == '/') {
          pos_ += 2;
          while (pos_ < src_.size() && src_[pos_] != '\n')
            pos_++;
          continue;
        }
        if (src_[pos_ + 1] == '*') {
          pos_ += 2;
          while (pos_ + 1 < src_.size() &&
                 !(src_[pos_] == '*' && src_[pos_ + 1] == '/'))
            pos_++;
          if (pos_ + 1 < src_.size())
            pos_ += 2;
          continue;
        }
      }
      break;
    }
  }

  token parseString() {
    pos_++;  // skip initial '"'
    std::string s;
    while (pos_ < src_.size()) {
      char c = src_[pos_++];
      if (c == '"')
        break;
      if (c == '\\' && pos_ < src_.size()) {
        char esc = src_[pos_++];
        if (esc == 'n')
          s += '\n';
        else if (esc == 't')
          s += '\t';
        else if (esc == 'r')
          s += '\r';
        else if (esc == '"')
          s += '"';
        else if (esc == '\\')
          s += '\\';
        else
          s += esc;
      } else {
        s += c;
      }
    }
    return {tokenType::kString, s, 0.0};
  }

  token parseNumber() {
    size_t start = pos_;
    if (src_[pos_] == '-')
      pos_++;
    while (pos_ < src_.size() &&
           (std::isdigit(static_cast<unsigned char>(src_[pos_])) ||
            src_[pos_] == '.')) {
      pos_++;
    }
    std::string str = src_.substr(start, pos_ - start);
    double val = 0.0;
    try {
      val = std::stod(str);
    } catch (...) {
    }
    return {tokenType::kNumber, str, val};
  }

  token parseIdent() {
    size_t start = pos_;
    while (pos_ < src_.size()) {
      char c = src_[pos_];
      if (std::isspace(static_cast<unsigned char>(c)) || c == '=' || c == '{' ||
          c == '}' || c == ';' || c == '"') {
        break;
      }
      pos_++;
    }
    return {tokenType::kIdent, src_.substr(start, pos_ - start), 0.0};
  }
};

namespace {

using tokenType = tokenizer::tokenType;

value toValue(const tokenizer::token& source) {
  value converted;
  if (source.type == tokenType::kString) {
    converted.valueType = value::type::kString;
    converted.stringVal = source.text;
  } else if (source.type == tokenType::kNumber) {
    converted.valueType = value::type::kNumber;
    converted.numberVal = source.numberVal;
    converted.stringVal = source.text;
  } else if (source.type == tokenType::kIdent) {
    bool isBoolean = source.text == "true" || source.text == "false";
    converted.valueType = isBoolean ? value::type::kBoolean : value::type::kString;
    converted.boolVal = source.text == "true";
    if (!isBoolean)
      converted.stringVal = source.text;
  }
  return converted;
}

// Recursive-descent reader over the token stream. Lenient by design: tokens
// it cannot place are skipped so a typo never discards the whole config.
class nodeParser {
public:
  explicit nodeParser(std::vector<tokenizer::token> tokens)
      : tokens_(std::move(tokens)) {}

  document parseDocument() {
    document doc;
    while (!isAt(tokenType::kEof)) {
      if (auto parsed = parseNode())
        doc.rootNodes.push_back(std::move(*parsed));
    }
    return doc;
  }

private:
  [[nodiscard]] bool hasMore() const { return index_ < tokens_.size(); }

  [[nodiscard]] bool isAt(tokenType type) const {
    return !hasMore() || tokens_[index_].type == type;
  }

  [[nodiscard]] bool isSeparator() const {
    return hasMore() && (tokens_[index_].type == tokenType::kNewline ||
                         tokens_[index_].type == tokenType::kSemicolon);
  }

  [[nodiscard]] bool endsNode(tokenType type) const {
    return type == tokenType::kNewline || type == tokenType::kSemicolon ||
           type == tokenType::kEof || type == tokenType::kRBrace;
  }

  [[nodiscard]] bool isPropertyStart() const {
    return tokens_[index_].type == tokenType::kIdent &&
           index_ + 1 < tokens_.size() &&
           tokens_[index_ + 1].type == tokenType::kEquals;
  }

  std::optional<node> parseNode() {
    while (isSeparator())
      index_++;
    if (isAt(tokenType::kEof) || isAt(tokenType::kRBrace))
      return std::nullopt;
    const auto& head = tokens_[index_++];
    if (head.type != tokenType::kIdent && head.type != tokenType::kString)
      return std::nullopt;

    node parsed;
    parsed.name = head.text;
    while (hasMore() && parseNodeItem(parsed))
      ;
    return parsed;
  }

  // Consumes one argument, property or child block; returns false once the
  // node is complete.
  bool parseNodeItem(node& parsed) {
    const auto& current = tokens_[index_];
    if (endsNode(current.type)) {
      index_++;
      return false;
    }
    if (current.type == tokenType::kLBrace) {
      index_++;
      parseChildren(parsed);
      return false;
    }
    if (isPropertyStart()) {
      std::string key = current.text;
      index_ += 2;
      if (hasMore())
        parsed.props[key] = toValue(tokens_[index_++]);
      return true;
    }
    parsed.args.push_back(toValue(current));
    index_++;
    return true;
  }

  void parseChildren(node& parent) {
    while (!isAt(tokenType::kRBrace) && !isAt(tokenType::kEof)) {
      if (auto child = parseNode())
        parent.children.push_back(std::move(*child));
    }
    if (hasMore() && tokens_[index_].type == tokenType::kRBrace)
      index_++;
  }

  std::vector<tokenizer::token> tokens_;
  size_t index_ = 0;
};

}  // namespace

document document::parse(const std::string& content, std::string&) {
  tokenizer lexer(content);
  std::vector<tokenizer::token> tokens;
  for (auto next = lexer.next();; next = lexer.next()) {
    tokens.push_back(next);
    if (next.type == tokenType::kEof)
      break;
  }
  return nodeParser(std::move(tokens)).parseDocument();
}

static void serializeNode(std::ostringstream& ss, const node& currentNode,
                          int indent) {
  std::string ind(indent * 4, ' ');
  ss << ind << currentNode.name;

  for (const auto& arg : currentNode.args) {
    ss << " ";
    if (arg.valueType == value::type::kString)
      ss << "\"" << arg.stringVal << "\"";
    else if (arg.valueType == value::type::kBoolean)
      ss << (arg.boolVal ? "true" : "false");
    else
      ss << arg.asString();
  }

  for (const auto& [k, v] : currentNode.props) {
    ss << " " << k << "=";
    if (v.valueType == value::type::kString)
      ss << "\"" << v.stringVal << "\"";
    else if (v.valueType == value::type::kBoolean)
      ss << (v.boolVal ? "true" : "false");
    else
      ss << v.asString();
  }

  if (!currentNode.children.empty()) {
    ss << " {\n";
    for (const auto& child : currentNode.children) {
      serializeNode(ss, child, indent + 1);
    }
    ss << ind << "}\n";
  } else {
    ss << "\n";
  }
}

std::string document::serialize(int indent) const {
  std::ostringstream ss;
  for (const auto& n : rootNodes) {
    serializeNode(ss, n, indent);
  }
  return ss.str();
}

}  // namespace catalist::kdl
