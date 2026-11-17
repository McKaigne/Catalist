#ifndef CATALIST_WEB_JSON_HPP_
#define CATALIST_WEB_JSON_HPP_

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "core/result.hpp"

// A small read-only JSON document model for the replies of the title search.
namespace catalist::json {

struct Member;

class Value {
public:
	using Array = std::vector<Value>;
	using Object = std::vector<Member>;

	Value() = default;
	Value(std::nullptr_t) {}
	Value(bool flag) : data_(flag) {}
	Value(double number) : data_(number) {}
	Value(std::string text) : data_(std::move(text)) {}
	Value(Array items) : data_(std::move(items)) {}
	Value(Object members) : data_(std::move(members)) {}

	[[nodiscard]] bool IsNull() const;
	[[nodiscard]] bool IsArray() const;

	[[nodiscard]] std::optional<double> AsNumber() const;
	[[nodiscard]] std::optional<bool> AsBool() const;
	// Whole numbers only; 3.5 or a string is absent.
	[[nodiscard]] std::optional<long long> AsInteger() const;
	// A count, an id or a total: a whole number above 0, else 0.
	[[nodiscard]] int AsCount() const;
	[[nodiscard]] std::optional<std::string> AsString() const;

	// A missing key, or indexing something that is not an object, yields null
	// so that lookups along a path can be chained.
	[[nodiscard]] const Value& operator[](std::string_view key) const;
	// Elements of an array; empty for any other kind.
	[[nodiscard]] const Array& Items() const;

private:
	std::variant<std::nullptr_t, bool, double, std::string, Array, Object> data_;
};

struct Member {
	std::string key;
	Value item;
};

[[nodiscard]] Result<Value> Parse(std::string_view text);

}  // namespace catalist::json

#endif  // CATALIST_WEB_JSON_HPP_
