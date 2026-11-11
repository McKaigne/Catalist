#ifndef CATALIST_JSON_HPP_
#define CATALIST_JSON_HPP_

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "result.hpp"

// A small read-only JSON document model for the replies of the title search.
namespace catalist::json {

struct member;

class value {
public:
  using array = std::vector<value>;
  using object = std::vector<member>;

  value() = default;
  value(std::nullptr_t) {}
  value(bool flag) : data_(flag) {}
  value(double number) : data_(number) {}
  value(std::string text) : data_(std::move(text)) {}
  value(array items) : data_(std::move(items)) {}
  value(object members) : data_(std::move(members)) {}

  [[nodiscard]] bool isNull() const;
  [[nodiscard]] bool isArray() const;

  [[nodiscard]] std::optional<double> asNumber() const;
  // Whole numbers only; 3.5 or a string is absent.
  [[nodiscard]] std::optional<long long> asInteger() const;
  [[nodiscard]] std::optional<std::string> asString() const;

  // A missing key, or indexing something that is not an object, yields null
  // so that lookups along a path can be chained.
  [[nodiscard]] const value& operator[](std::string_view key) const;
  // Elements of an array; empty for any other kind.
  [[nodiscard]] const array& items() const;

private:
  std::variant<std::nullptr_t, bool, double, std::string, array, object> data_;
};

struct member {
  std::string key;
  value item;
};

[[nodiscard]] result<value> parse(std::string_view text);

}  // namespace catalist::json

#endif  // CATALIST_JSON_HPP_
