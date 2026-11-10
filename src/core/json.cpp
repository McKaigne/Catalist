#include "catalist/core/json.hpp"

#include <charconv>
#include <cmath>
#include <cstdint>

namespace catalist::json {

namespace {

// Deep enough for any API reply we read; stops hostile input from exhausting
// the stack.
constexpr int kMaxDepth = 64;

const value& nullValue() {
  static const value kNull;
  return kNull;
}

void appendUtf8(std::string& out, std::uint32_t codePoint) {
  if (codePoint < 0x80) {
    out += static_cast<char>(codePoint);
  } else if (codePoint < 0x800) {
    out += static_cast<char>(0xC0 | (codePoint >> 6));
    out += static_cast<char>(0x80 | (codePoint & 0x3F));
  } else if (codePoint < 0x10000) {
    out += static_cast<char>(0xE0 | (codePoint >> 12));
    out += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (codePoint & 0x3F));
  } else {
    out += static_cast<char>(0xF0 | (codePoint >> 18));
    out += static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F));
    out += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (codePoint & 0x3F));
  }
}

std::optional<char> simpleEscape(char escaped) {
  switch (escaped) {
    case '"': return '"';
    case '\\': return '\\';
    case '/': return '/';
    case 'b': return '\b';
    case 'f': return '\f';
    case 'n': return '\n';
    case 'r': return '\r';
    case 't': return '\t';
    default: return std::nullopt;
  }
}

bool isHighSurrogate(std::uint32_t unit) {
  return unit >= 0xD800 && unit <= 0xDBFF;
}

bool isLowSurrogate(std::uint32_t unit) {
  return unit >= 0xDC00 && unit <= 0xDFFF;
}

class parser {
public:
  explicit parser(std::string_view text) : text_(text) {}

  result<value> parseDocument() {
    auto parsed = parseValue(0);
    if (!parsed)
      return result<value>::failure(error_);
    skipSpace();
    if (pos_ != text_.size())
      return result<value>::failure("unexpected text after the JSON value");
    return result<value>::success(std::move(*parsed));
  }

private:
  std::optional<value> fail(std::string message) {
    if (error_.empty())
      error_ = std::move(message) + " at offset " + std::to_string(pos_);
    return std::nullopt;
  }

  void skipSpace() {
    while (pos_ < text_.size() && (text_[pos_] == ' ' || text_[pos_] == '\n' ||
                                   text_[pos_] == '\r' || text_[pos_] == '\t'))
      ++pos_;
  }

  bool consume(char expected) {
    skipSpace();
    if (pos_ >= text_.size() || text_[pos_] != expected)
      return false;
    ++pos_;
    return true;
  }

  bool consumeWord(std::string_view word) {
    if (text_.substr(pos_, word.size()) != word)
      return false;
    pos_ += word.size();
    return true;
  }

  std::optional<value> parseValue(int depth) {
    if (depth > kMaxDepth)
      return fail("nesting too deep");
    skipSpace();
    if (pos_ >= text_.size())
      return fail("unexpected end of input");
    char lead = text_[pos_];
    if (lead == '{')
      return parseObject(depth);
    if (lead == '[')
      return parseArray(depth);
    if (lead == '"') {
      auto text = parseString();
      return text ? std::optional<value>(std::move(*text)) : std::nullopt;
    }
    if (consumeWord("true"))
      return value(true);
    if (consumeWord("false"))
      return value(false);
    if (consumeWord("null"))
      return value();
    return parseNumber();
  }

  std::optional<value> parseNumber() {
    size_t start = pos_;
    while (pos_ < text_.size() &&
           std::string_view("+-0123456789.eE").find(text_[pos_]) !=
               std::string_view::npos)
      ++pos_;
    double number = 0.0;
    auto [end, error] =
        std::from_chars(text_.data() + start, text_.data() + pos_, number);
    if (start == pos_ || error != std::errc() || end != text_.data() + pos_)
      return fail("invalid number");
    return value(number);
  }

  std::optional<std::uint32_t> parseHex4() {
    if (pos_ + 4 > text_.size())
      return std::nullopt;
    std::uint32_t unit = 0;
    auto [end, error] =
        std::from_chars(text_.data() + pos_, text_.data() + pos_ + 4, unit, 16);
    if (error != std::errc() || end != text_.data() + pos_ + 4)
      return std::nullopt;
    pos_ += 4;
    return unit;
  }

  // Reads the digits after "\u", joining a surrogate pair into one code point.
  std::optional<std::uint32_t> parseUnicodeEscape() {
    auto unit = parseHex4();
    if (!unit || !isHighSurrogate(*unit))
      return unit;
    if (!consumeWord("\\u"))
      return std::nullopt;
    auto low = parseHex4();
    if (!low || !isLowSurrogate(*low))
      return std::nullopt;
    return 0x10000 + ((*unit - 0xD800) << 10) + (*low - 0xDC00);
  }

  bool appendEscape(std::string& out) {
    if (pos_ >= text_.size())
      return false;
    char escaped = text_[pos_++];
    if (auto simple = simpleEscape(escaped)) {
      out += *simple;
      return true;
    }
    if (escaped != 'u')
      return false;
    auto codePoint = parseUnicodeEscape();
    if (codePoint)
      appendUtf8(out, *codePoint);
    return codePoint.has_value();
  }

  std::optional<std::string> parseString() {
    ++pos_;
    std::string out;
    while (pos_ < text_.size() && text_[pos_] != '"') {
      char current = text_[pos_++];
      if (current != '\\') {
        out += current;
        continue;
      }
      if (!appendEscape(out)) {
        fail("invalid escape in string");
        return std::nullopt;
      }
    }
    if (pos_ >= text_.size()) {
      fail("unterminated string");
      return std::nullopt;
    }
    ++pos_;
    return out;
  }

  std::optional<value> parseArray(int depth) {
    ++pos_;
    value::array items;
    if (consume(']'))
      return value(std::move(items));
    do {
      auto item = parseValue(depth + 1);
      if (!item)
        return std::nullopt;
      items.push_back(std::move(*item));
    } while (consume(','));
    if (!consume(']'))
      return fail("expected ',' or ']'");
    return value(std::move(items));
  }

  std::optional<member> parseMember(int depth) {
    skipSpace();
    if (pos_ >= text_.size() || text_[pos_] != '"') {
      fail("expected a key");
      return std::nullopt;
    }
    auto key = parseString();
    if (!key)
      return std::nullopt;
    if (!consume(':')) {
      fail("expected ':'");
      return std::nullopt;
    }
    auto item = parseValue(depth + 1);
    if (!item)
      return std::nullopt;
    return member{std::move(*key), std::move(*item)};
  }

  std::optional<value> parseObject(int depth) {
    ++pos_;
    value::object members;
    if (consume('}'))
      return value(std::move(members));
    do {
      auto parsed = parseMember(depth);
      if (!parsed)
        return std::nullopt;
      members.push_back(std::move(*parsed));
    } while (consume(','));
    if (!consume('}'))
      return fail("expected ',' or '}'");
    return value(std::move(members));
  }

  std::string_view text_;
  size_t pos_ = 0;
  std::string error_;
};

}  // namespace

bool value::isNull() const {
  return std::holds_alternative<std::nullptr_t>(data_);
}

bool value::isObject() const {
  return std::holds_alternative<object>(data_);
}

bool value::isArray() const {
  return std::holds_alternative<array>(data_);
}

std::optional<bool> value::asBool() const {
  if (auto flag = std::get_if<bool>(&data_))
    return *flag;
  return std::nullopt;
}

std::optional<double> value::asNumber() const {
  if (auto number = std::get_if<double>(&data_))
    return *number;
  return std::nullopt;
}

std::optional<long long> value::asInteger() const {
  auto number = asNumber();
  if (!number || std::trunc(*number) != *number)
    return std::nullopt;
  return static_cast<long long>(*number);
}

std::optional<std::string> value::asString() const {
  if (auto text = std::get_if<std::string>(&data_))
    return *text;
  return std::nullopt;
}

const value& value::operator[](std::string_view key) const {
  for (const auto& entry : members()) {
    if (entry.key == key)
      return entry.item;
  }
  return nullValue();
}

const value::array& value::items() const {
  static const array kEmpty;
  auto found = std::get_if<array>(&data_);
  return found ? *found : kEmpty;
}

const value::object& value::members() const {
  static const object kEmpty;
  auto found = std::get_if<object>(&data_);
  return found ? *found : kEmpty;
}

result<value> parse(std::string_view text) {
  return parser(text).parseDocument();
}

}  // namespace catalist::json
