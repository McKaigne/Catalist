#include "catalist/kdl.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>

namespace catalist::kdl {

std::string value::asString() const {
  if (valueType == type::String)
    return stringVal;
  if (valueType == type::Number) {
    if (numberVal == static_cast<long long>(numberVal)) {
      return std::to_string(static_cast<long long>(numberVal));
    }
    return std::to_string(numberVal);
  }
  if (valueType == type::Boolean)
    return boolVal ? "true" : "false";
  return "";
}

int value::asInt(int defaultVal) const {
  if (valueType == type::Number)
    return static_cast<int>(numberVal);
  if (valueType == type::String) {
    try {
      return std::stoi(stringVal);
    } catch (...) {
    }
  }
  return defaultVal;
}

double value::asDouble(double defaultVal) const {
  if (valueType == type::Number)
    return numberVal;
  if (valueType == type::String) {
    try {
      return std::stod(stringVal);
    } catch (...) {
    }
  }
  return defaultVal;
}

bool value::asBool(bool defaultVal) const {
  if (valueType == type::Boolean)
    return boolVal;
  if (valueType == type::String) {
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
    Ident,
    String,
    Number,
    Equals,
    LBrace,
    RBrace,
    Semicolon,
    Newline,
    Eof
  };

  struct token {
    tokenType type = tokenType::Eof;
    std::string text;
    double numberVal = 0.0;
  };

  token next() {
    skipWhitespaceAndComments();
    if (pos_ >= src_.size())
      return {tokenType::Eof, "", 0.0};

    char c = src_[pos_];

    if (c == '\n' || c == '\r') {
      while (pos_ < src_.size() && (src_[pos_] == '\n' || src_[pos_] == '\r'))
        pos_++;
      return {tokenType::Newline, "\n", 0.0};
    }
    if (c == ';') {
      pos_++;
      return {tokenType::Semicolon, ";", 0.0};
    }
    if (c == '=') {
      pos_++;
      return {tokenType::Equals, "=", 0.0};
    }
    if (c == '{') {
      pos_++;
      return {tokenType::LBrace, "{", 0.0};
    }
    if (c == '}') {
      pos_++;
      return {tokenType::RBrace, "}", 0.0};
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
    return {tokenType::String, s, 0.0};
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
    return {tokenType::Number, str, val};
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
    return {tokenType::Ident, src_.substr(start, pos_ - start), 0.0};
  }
};

document document::parse(const std::string& content, std::string& outErr) {
  document doc;
  tokenizer lexer(content);

  std::vector<tokenizer::token> tokens;
  while (true) {
    auto tok = lexer.next();
    tokens.push_back(tok);
    if (tok.type == tokenizer::tokenType::Eof)
      break;
  }

  size_t idx = 0;

  auto parseNode = [&](auto& self) -> std::optional<node> {
    while (idx < tokens.size() &&
           (tokens[idx].type == tokenizer::tokenType::Newline ||
            tokens[idx].type == tokenizer::tokenType::Semicolon)) {
      idx++;
    }
    if (idx >= tokens.size() || tokens[idx].type == tokenizer::tokenType::Eof ||
        tokens[idx].type == tokenizer::tokenType::RBrace) {
      return std::nullopt;
    }

    if (tokens[idx].type != tokenizer::tokenType::Ident &&
        tokens[idx].type != tokenizer::tokenType::String) {
      idx++;
      return std::nullopt;
    }

    node currentNode;
    currentNode.name = tokens[idx++].text;

    while (idx < tokens.size()) {
      auto t = tokens[idx];
      if (t.type == tokenizer::tokenType::Newline ||
          t.type == tokenizer::tokenType::Semicolon ||
          t.type == tokenizer::tokenType::Eof ||
          t.type == tokenizer::tokenType::RBrace) {
        idx++;
        break;
      }

      if (t.type == tokenizer::tokenType::LBrace) {
        idx++;
        while (idx < tokens.size() &&
               tokens[idx].type != tokenizer::tokenType::RBrace &&
               tokens[idx].type != tokenizer::tokenType::Eof) {
          auto child = self(self);
          if (child.has_value()) {
            currentNode.children.push_back(child.value());
          }
        }
        if (idx < tokens.size() &&
            tokens[idx].type == tokenizer::tokenType::RBrace) {
          idx++;
        }
        break;
      }

      // Check if property: ident = value
      if (t.type == tokenizer::tokenType::Ident && idx + 1 < tokens.size() &&
          tokens[idx + 1].type == tokenizer::tokenType::Equals) {
        std::string propKey = t.text;
        idx += 2;
        if (idx < tokens.size()) {
          auto valTok = tokens[idx++];
          value v;
          if (valTok.type == tokenizer::tokenType::String) {
            v.valueType = value::type::String;
            v.stringVal = valTok.text;
          } else if (valTok.type == tokenizer::tokenType::Number) {
            v.valueType = value::type::Number;
            v.numberVal = valTok.numberVal;
            v.stringVal = valTok.text;
          } else if (valTok.type == tokenizer::tokenType::Ident) {
            if (valTok.text == "true" || valTok.text == "false") {
              v.valueType = value::type::Boolean;
              v.boolVal = (valTok.text == "true");
            } else {
              v.valueType = value::type::String;
              v.stringVal = valTok.text;
            }
          }
          currentNode.props[propKey] = v;
        }
        continue;
      }

      // Positional argument
      value v;
      if (t.type == tokenizer::tokenType::String) {
        v.valueType = value::type::String;
        v.stringVal = t.text;
      } else if (t.type == tokenizer::tokenType::Number) {
        v.valueType = value::type::Number;
        v.numberVal = t.numberVal;
        v.stringVal = t.text;
      } else if (t.type == tokenizer::tokenType::Ident) {
        if (t.text == "true" || t.text == "false") {
          v.valueType = value::type::Boolean;
          v.boolVal = (t.text == "true");
        } else {
          v.valueType = value::type::String;
          v.stringVal = t.text;
        }
      }
      currentNode.args.push_back(v);
      idx++;
    }

    return currentNode;
  };

  while (idx < tokens.size() && tokens[idx].type != tokenizer::tokenType::Eof) {
    auto n = parseNode(parseNode);
    if (n.has_value()) {
      doc.rootNodes.push_back(n.value());
    }
  }

  (void)outErr;
  return doc;
}

static void serializeNode(std::ostringstream& ss, const node& currentNode,
                          int indent) {
  std::string ind(indent * 4, ' ');
  ss << ind << currentNode.name;

  for (const auto& arg : currentNode.args) {
    ss << " ";
    if (arg.valueType == value::type::String)
      ss << "\"" << arg.stringVal << "\"";
    else if (arg.valueType == value::type::Boolean)
      ss << (arg.boolVal ? "true" : "false");
    else
      ss << arg.asString();
  }

  for (const auto& [k, v] : currentNode.props) {
    ss << " " << k << "=";
    if (v.valueType == value::type::String)
      ss << "\"" << v.stringVal << "\"";
    else if (v.valueType == value::type::Boolean)
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
