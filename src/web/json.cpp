#include "web/json.hpp"

#include <charconv>
#include <cmath>
#include <cstdint>
#include <format>
#include <limits>

namespace catalist::json {

namespace {

// Deep enough for any API reply we read; stops hostile input from exhausting
// the stack.
constexpr int kMaxDepth = 64;

const Value& NullValue() {
	static const Value kNull;
	return kNull;
}

void AppendUtf8(std::string& out, std::uint32_t code_point) {
	if (code_point < 0x80) {
		out += static_cast<char>(code_point);
	} else if (code_point < 0x800) {
		out += static_cast<char>(0xC0 | (code_point >> 6));
		out += static_cast<char>(0x80 | (code_point & 0x3F));
	} else if (code_point < 0x10000) {
		out += static_cast<char>(0xE0 | (code_point >> 12));
		out += static_cast<char>(0x80 | ((code_point >> 6) & 0x3F));
		out += static_cast<char>(0x80 | (code_point & 0x3F));
	} else {
		out += static_cast<char>(0xF0 | (code_point >> 18));
		out += static_cast<char>(0x80 | ((code_point >> 12) & 0x3F));
		out += static_cast<char>(0x80 | ((code_point >> 6) & 0x3F));
		out += static_cast<char>(0x80 | (code_point & 0x3F));
	}
}

std::optional<char> SimpleEscape(char escaped) {
	switch (escaped) {
		case '"':
			return '"';
		case '\\':
			return '\\';
		case '/':
			return '/';
		case 'b':
			return '\b';
		case 'f':
			return '\f';
		case 'n':
			return '\n';
		case 'r':
			return '\r';
		case 't':
			return '\t';
		default:
			return std::nullopt;
	}
}

bool IsHighSurrogate(std::uint32_t unit) {
	return unit >= 0xD800 and unit <= 0xDBFF;
}

bool IsLowSurrogate(std::uint32_t unit) {
	return unit >= 0xDC00 and unit <= 0xDFFF;
}

class Parser {
public:
	explicit Parser(std::string_view text) : text_(text) {}

	Result<Value> ParseDocument() {
		auto parsed = ParseValue(0);
		if (not parsed)
			return std::unexpected(error_);
		SkipSpace();
		if (pos_ != text_.size())
			return std::unexpected("unexpected text after the JSON value");
		return std::move(*parsed);
	}

private:
	std::optional<Value> Fail(std::string message) {
		if (error_.empty())
			error_ = std::format("{} at offset {}", message, pos_);
		return std::nullopt;
	}

	void SkipSpace() {
		while (pos_ < text_.size() and (text_[pos_] == ' ' or text_[pos_] == '\n' or
		                                text_[pos_] == '\r' or text_[pos_] == '\t'))
			++pos_;
	}

	bool Consume(char expected) {
		SkipSpace();
		if (pos_ >= text_.size() or text_[pos_] != expected)
			return false;
		++pos_;
		return true;
	}

	bool ConsumeWord(std::string_view word) {
		if (text_.substr(pos_, word.size()) != word)
			return false;
		pos_ += word.size();
		return true;
	}

	std::optional<Value> ParseValue(int depth) {
		if (depth > kMaxDepth)
			return Fail("nesting too deep");
		SkipSpace();
		if (pos_ >= text_.size())
			return Fail("unexpected end of input");
		char lead = text_[pos_];
		if (lead == '{')
			return ParseObject(depth);
		if (lead == '[')
			return ParseArray(depth);
		if (lead == '"') {
			auto text = ParseString();
			return text ? std::optional<Value>(std::move(*text)) : std::nullopt;
		}
		if (ConsumeWord("true"))
			return Value(true);
		if (ConsumeWord("false"))
			return Value(false);
		if (ConsumeWord("null"))
			return Value();
		return ParseNumber();
	}

	std::optional<Value> ParseNumber() {
		size_t start = pos_;
		while (pos_ < text_.size() and
		       std::string_view("+-0123456789.eE").find(text_[pos_]) != std::string_view::npos)
			++pos_;
		double number = 0.0;
		auto [end, error] = std::from_chars(text_.data() + start, text_.data() + pos_, number);
		if (start == pos_ or error != std::errc() or end != text_.data() + pos_)
			return Fail("invalid number");
		return Value(number);
	}

	std::optional<std::uint32_t> ParseHex4() {
		if (pos_ + 4 > text_.size())
			return std::nullopt;
		std::uint32_t unit = 0;
		auto [end, error] = std::from_chars(text_.data() + pos_, text_.data() + pos_ + 4, unit, 16);
		if (error != std::errc() or end != text_.data() + pos_ + 4)
			return std::nullopt;
		pos_ += 4;
		return unit;
	}

	// Reads the digits after "\u", joining a surrogate pair into one code point.
	std::optional<std::uint32_t> ParseUnicodeEscape() {
		auto unit = ParseHex4();
		if (not unit or not IsHighSurrogate(*unit))
			return unit;
		if (not ConsumeWord("\\u"))
			return std::nullopt;
		auto low = ParseHex4();
		if (not low or not IsLowSurrogate(*low))
			return std::nullopt;
		return 0x10000 + ((*unit - 0xD800) << 10) + (*low - 0xDC00);
	}

	bool AppendEscape(std::string& out) {
		if (pos_ >= text_.size())
			return false;
		char escaped = text_[pos_++];
		if (auto simple = SimpleEscape(escaped)) {
			out += *simple;
			return true;
		}
		if (escaped != 'u')
			return false;
		auto code_point = ParseUnicodeEscape();
		if (code_point)
			AppendUtf8(out, *code_point);
		return code_point.has_value();
	}

	std::optional<std::string> ParseString() {
		++pos_;
		std::string out;
		while (pos_ < text_.size() and text_[pos_] != '"') {
			char current = text_[pos_++];
			if (current != '\\') {
				out += current;
				continue;
			}
			if (not AppendEscape(out)) {
				Fail("invalid escape in string");
				return std::nullopt;
			}
		}
		if (pos_ >= text_.size()) {
			Fail("unterminated string");
			return std::nullopt;
		}
		++pos_;
		return out;
	}

	std::optional<Value> ParseArray(int depth) {
		++pos_;
		Value::Array items;
		if (Consume(']'))
			return Value(std::move(items));
		do {
			auto item = ParseValue(depth + 1);
			if (not item)
				return std::nullopt;
			items.push_back(std::move(*item));
		} while (Consume(','));
		if (not Consume(']'))
			return Fail("expected ',' or ']'");
		return Value(std::move(items));
	}

	std::optional<Member> ParseMember(int depth) {
		SkipSpace();
		if (pos_ >= text_.size() or text_[pos_] != '"') {
			Fail("expected a key");
			return std::nullopt;
		}
		auto key = ParseString();
		if (not key)
			return std::nullopt;
		if (not Consume(':')) {
			Fail("expected ':'");
			return std::nullopt;
		}
		auto item = ParseValue(depth + 1);
		if (not item)
			return std::nullopt;
		return Member{std::move(*key), std::move(*item)};
	}

	std::optional<Value> ParseObject(int depth) {
		++pos_;
		Value::Object members;
		if (Consume('}'))
			return Value(std::move(members));
		do {
			auto parsed = ParseMember(depth);
			if (not parsed)
				return std::nullopt;
			members.push_back(std::move(*parsed));
		} while (Consume(','));
		if (not Consume('}'))
			return Fail("expected ',' or '}'");
		return Value(std::move(members));
	}

	std::string_view text_;
	size_t pos_ = 0;
	std::string error_;
};

}  // namespace

bool Value::IsNull() const {
	return std::holds_alternative<std::nullptr_t>(data_);
}

bool Value::IsArray() const {
	return std::holds_alternative<Array>(data_);
}

std::optional<double> Value::AsNumber() const {
	if (auto number = std::get_if<double>(&data_))
		return *number;
	return std::nullopt;
}

std::optional<bool> Value::AsBool() const {
	if (auto flag = std::get_if<bool>(&data_))
		return *flag;
	return std::nullopt;
}

std::optional<long long> Value::AsInteger() const {
	auto number = AsNumber();
	constexpr double kMinInt64 = -9223372036854775808.0;
	constexpr double kMaxInt64PlusOne = 9223372036854775808.0;
	if (not number or not std::isfinite(*number) or std::trunc(*number) != *number or
	    *number < kMinInt64 or *number >= kMaxInt64PlusOne)
		return std::nullopt;
	return static_cast<long long>(*number);
}

int Value::AsCount() const {
	auto count = AsInteger();
	if (not count or *count <= 0 or *count > std::numeric_limits<int>::max())
		return 0;
	return static_cast<int>(*count);
}

std::optional<std::string> Value::AsString() const {
	if (auto text = std::get_if<std::string>(&data_))
		return *text;
	return std::nullopt;
}

const Value& Value::operator[](std::string_view key) const {
	auto members = std::get_if<Object>(&data_);
	if (not members)
		return NullValue();
	for (const auto& entry : *members) {
		if (entry.key == key)
			return entry.item;
	}
	return NullValue();
}

const Value::Array& Value::Items() const {
	static const Array kEmpty;
	auto found = std::get_if<Array>(&data_);
	return found ? *found : kEmpty;
}

Result<Value> Parse(std::string_view text) {
	return Parser(text).ParseDocument();
}

}  // namespace catalist::json
