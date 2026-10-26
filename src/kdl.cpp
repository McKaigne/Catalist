#include "catalist/kdl.hpp"
#include <sstream>
#include <cctype>
#include <algorithm>
#include <iomanip>

namespace catalist::kdl {

std::string Value::asString() const {
  if (type == Type::String) return stringVal;
  if (type == Type::Number) {
    if (numberVal == static_cast<long long>(numberVal)) {
      return std::to_string(static_cast<long long>(numberVal));
    }
    return std::to_string(numberVal);
  }
  if (type == Type::Boolean) return boolVal ? "true" : "false";
  return "";
}

int Value::asInt(int defaultVal) const {
  if (type == Type::Number) return static_cast<int>(numberVal);
  if (type == Type::String) {
    try { return std::stoi(stringVal); } catch (...) {}
  }
  return defaultVal;
}

double Value::asDouble(double defaultVal) const {
  if (type == Type::Number) return numberVal;
  if (type == Type::String) {
    try { return std::stod(stringVal); } catch (...) {}
  }
  return defaultVal;
}

bool Value::asBool(bool defaultVal) const {
  if (type == Type::Boolean) return boolVal;
  if (type == Type::String) {
    std::string s = stringVal;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    if (s == "true" || s == "1" || s == "yes") return true;
    if (s == "false" || s == "0" || s == "no") return false;
  }
  return defaultVal;
}

bool Node::hasProp(const std::string& key) const {
  return props.find(key) != props.end();
}

Value Node::getProp(const std::string& key, const Value& defaultVal) const {
  auto it = props.find(key);
  if (it != props.end()) return it->second;
  return defaultVal;
}

std::string Node::getPropString(const std::string& key, const std::string& defaultVal) const {
  auto it = props.find(key);
  if (it != props.end()) return it->second.asString();
  return defaultVal;
}

int Node::getPropInt(const std::string& key, int defaultVal) const {
  auto it = props.find(key);
  if (it != props.end()) return it->second.asInt(defaultVal);
  return defaultVal;
}

bool Node::getPropBool(const std::string& key, bool defaultVal) const {
  auto it = props.find(key);
  if (it != props.end()) return it->second.asBool(defaultVal);
  return defaultVal;
}

Value Node::getArg(size_t index, const Value& defaultVal) const {
  if (index < args.size()) return args[index];
  return defaultVal;
}

std::string Node::getArgString(size_t index, const std::string& defaultVal) const {
  if (index < args.size()) return args[index].asString();
  return defaultVal;
}

std::vector<Node> Node::getChildren(const std::string& childName) const {
  std::vector<Node> res;
  for (const auto& child : children) {
    if (child.name == childName) res.push_back(child);
  }
  return res;
}

std::optional<Node> Node::getChild(const std::string& childName) const {
  for (const auto& child : children) {
    if (child.name == childName) return child;
  }
  return std::nullopt;
}

std::vector<Node> Document::getNodes(const std::string& name) const {
  std::vector<Node> res;
  for (const auto& n : rootNodes) {
    if (n.name == name) res.push_back(n);
  }
  return res;
}

std::optional<Node> Document::getNode(const std::string& name) const {
  for (const auto& n : rootNodes) {
    if (n.name == name) return n;
  }
  return std::nullopt;
}

class Tokenizer {
public:
  explicit Tokenizer(std::string src) : src_(std::move(src)), pos_(0) {}

  enum class TokenType {
    Ident, String, Number, Equals, LBrace, RBrace, Semicolon, Newline, Eof
  };

  struct Token {
    TokenType type = TokenType::Eof;
    std::string text;
    double numberVal = 0.0;
  };

  Token next() {
    skipWhitespaceAndComments();
    if (pos_ >= src_.size()) return {TokenType::Eof, "", 0.0};

    char c = src_[pos_];

    if (c == '\n' || c == '\r') {
      while (pos_ < src_.size() && (src_[pos_] == '\n' || src_[pos_] == '\r')) pos_++;
      return {TokenType::Newline, "\n", 0.0};
    }
    if (c == ';') { pos_++; return {TokenType::Semicolon, ";", 0.0}; }
    if (c == '=') { pos_++; return {TokenType::Equals, "=", 0.0}; }
    if (c == '{') { pos_++; return {TokenType::LBrace, "{", 0.0}; }
    if (c == '}') { pos_++; return {TokenType::RBrace, "}", 0.0}; }

    if (c == '"') return parseString();

    if (std::isdigit(static_cast<unsigned char>(c)) || (c == '-' && pos_ + 1 < src_.size() && std::isdigit(static_cast<unsigned char>(src_[pos_ + 1])))) {
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
          while (pos_ < src_.size() && src_[pos_] != '\n') pos_++;
          continue;
        }
        if (src_[pos_ + 1] == '*') {
          pos_ += 2;
          while (pos_ + 1 < src_.size() && !(src_[pos_] == '*' && src_[pos_ + 1] == '/')) pos_++;
          if (pos_ + 1 < src_.size()) pos_ += 2;
          continue;
        }
      }
      break;
    }
  }

  Token parseString() {
    pos_++; // skip initial '"'
    std::string s;
    while (pos_ < src_.size()) {
      char c = src_[pos_++];
      if (c == '"') break;
      if (c == '\\' && pos_ < src_.size()) {
        char esc = src_[pos_++];
        if (esc == 'n') s += '\n';
        else if (esc == 't') s += '\t';
        else if (esc == 'r') s += '\r';
        else if (esc == '"') s += '"';
        else if (esc == '\\') s += '\\';
        else s += esc;
      } else {
        s += c;
      }
    }
    return {TokenType::String, s, 0.0};
  }

  Token parseNumber() {
    size_t start = pos_;
    if (src_[pos_] == '-') pos_++;
    while (pos_ < src_.size() && (std::isdigit(static_cast<unsigned char>(src_[pos_])) || src_[pos_] == '.')) {
      pos_++;
    }
    std::string str = src_.substr(start, pos_ - start);
    double val = 0.0;
    try { val = std::stod(str); } catch (...) {}
    return {TokenType::Number, str, val};
  }

  Token parseIdent() {
    size_t start = pos_;
    while (pos_ < src_.size()) {
      char c = src_[pos_];
      if (std::isspace(static_cast<unsigned char>(c)) || c == '=' || c == '{' || c == '}' || c == ';' || c == '"') {
        break;
      }
      pos_++;
    }
    return {TokenType::Ident, src_.substr(start, pos_ - start), 0.0};
  }
};

Document Document::parse(const std::string& content, std::string& outErr) {
  Document doc;
  Tokenizer tokenizer(content);

  std::vector<Tokenizer::Token> tokens;
  while (true) {
    auto tok = tokenizer.next();
    tokens.push_back(tok);
    if (tok.type == Tokenizer::TokenType::Eof) break;
  }

  size_t idx = 0;

  auto parseNode = [&](auto& self) -> std::optional<Node> {
    while (idx < tokens.size() && (tokens[idx].type == Tokenizer::TokenType::Newline || tokens[idx].type == Tokenizer::TokenType::Semicolon)) {
      idx++;
    }
    if (idx >= tokens.size() || tokens[idx].type == Tokenizer::TokenType::Eof || tokens[idx].type == Tokenizer::TokenType::RBrace) {
      return std::nullopt;
    }

    if (tokens[idx].type != Tokenizer::TokenType::Ident && tokens[idx].type != Tokenizer::TokenType::String) {
      idx++;
      return std::nullopt;
    }

    Node node;
    node.name = tokens[idx++].text;

    while (idx < tokens.size()) {
      auto t = tokens[idx];
      if (t.type == Tokenizer::TokenType::Newline || t.type == Tokenizer::TokenType::Semicolon || t.type == Tokenizer::TokenType::Eof || t.type == Tokenizer::TokenType::RBrace) {
        idx++;
        break;
      }

      if (t.type == Tokenizer::TokenType::LBrace) {
        idx++;
        while (idx < tokens.size() && tokens[idx].type != Tokenizer::TokenType::RBrace && tokens[idx].type != Tokenizer::TokenType::Eof) {
          auto child = self(self);
          if (child.has_value()) {
            node.children.push_back(child.value());
          }
        }
        if (idx < tokens.size() && tokens[idx].type == Tokenizer::TokenType::RBrace) {
          idx++;
        }
        break;
      }

      // Check if property: ident = value
      if (t.type == Tokenizer::TokenType::Ident && idx + 1 < tokens.size() && tokens[idx + 1].type == Tokenizer::TokenType::Equals) {
        std::string propKey = t.text;
        idx += 2;
        if (idx < tokens.size()) {
          auto valTok = tokens[idx++];
          Value v;
          if (valTok.type == Tokenizer::TokenType::String) {
            v.type = Value::Type::String;
            v.stringVal = valTok.text;
          } else if (valTok.type == Tokenizer::TokenType::Number) {
            v.type = Value::Type::Number;
            v.numberVal = valTok.numberVal;
            v.stringVal = valTok.text;
          } else if (valTok.type == Tokenizer::TokenType::Ident) {
            if (valTok.text == "true" || valTok.text == "false") {
              v.type = Value::Type::Boolean;
              v.boolVal = (valTok.text == "true");
            } else {
              v.type = Value::Type::String;
              v.stringVal = valTok.text;
            }
          }
          node.props[propKey] = v;
        }
        continue;
      }

      // Positional argument
      Value v;
      if (t.type == Tokenizer::TokenType::String) {
        v.type = Value::Type::String;
        v.stringVal = t.text;
      } else if (t.type == Tokenizer::TokenType::Number) {
        v.type = Value::Type::Number;
        v.numberVal = t.numberVal;
        v.stringVal = t.text;
      } else if (t.type == Tokenizer::TokenType::Ident) {
        if (t.text == "true" || t.text == "false") {
          v.type = Value::Type::Boolean;
          v.boolVal = (t.text == "true");
        } else {
          v.type = Value::Type::String;
          v.stringVal = t.text;
        }
      }
      node.args.push_back(v);
      idx++;
    }

    return node;
  };

  while (idx < tokens.size() && tokens[idx].type != Tokenizer::TokenType::Eof) {
    auto n = parseNode(parseNode);
    if (n.has_value()) {
      doc.rootNodes.push_back(n.value());
    }
  }

  (void)outErr;
  return doc;
}

static void serializeNode(std::ostringstream& ss, const Node& node, int indent) {
  std::string ind(indent * 4, ' ');
  ss << ind << node.name;

  for (const auto& arg : node.args) {
    ss << " ";
    if (arg.type == Value::Type::String) ss << "\"" << arg.stringVal << "\"";
    else if (arg.type == Value::Type::Boolean) ss << (arg.boolVal ? "true" : "false");
    else ss << arg.asString();
  }

  for (const auto& [k, v] : node.props) {
    ss << " " << k << "=";
    if (v.type == Value::Type::String) ss << "\"" << v.stringVal << "\"";
    else if (v.type == Value::Type::Boolean) ss << (v.boolVal ? "true" : "false");
    else ss << v.asString();
  }

  if (!node.children.empty()) {
    ss << " {\n";
    for (const auto& child : node.children) {
      serializeNode(ss, child, indent + 1);
    }
    ss << ind << "}\n";
  } else {
    ss << "\n";
  }
}

std::string Document::serialize(int indent) const {
  std::ostringstream ss;
  for (const auto& n : rootNodes) {
    serializeNode(ss, n, indent);
  }
  return ss.str();
}

} // namespace catalist::kdl
