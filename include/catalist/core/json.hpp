#ifndef CATALIST_CORE_JSON_HPP_
#define CATALIST_CORE_JSON_HPP_

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "catalist/core/result.hpp"

// A small JSON document model for the settings file and the nested replies of
// the MyAnimeList and AniList APIs.
namespace catalist::json {

struct member;

class value {
public:
  using array = std::vector<value>;
  using object = std::vector<member>;

  value() = default;
  value(std::nullptr_t) {}
  value(bool flag) : data_(flag) {}
  value(int number) : data_(static_cast<double>(number)) {}
  value(long long number) : data_(static_cast<double>(number)) {}
  value(double number) : data_(number) {}
  value(const char* text) : data_(std::string(text)) {}
  value(std::string text) : data_(std::move(text)) {}
  value(array items) : data_(std::move(items)) {}
  value(object members) : data_(std::move(members)) {}

  [[nodiscard]] bool isNull() const;
  [[nodiscard]] bool isObject() const;
  [[nodiscard]] bool isArray() const;

  [[nodiscard]] std::optional<bool> asBool() const;
  [[nodiscard]] std::optional<double> asNumber() const;
  // Whole numbers only; 3.5 or a string is absent.
  [[nodiscard]] std::optional<long long> asInteger() const;
  [[nodiscard]] std::optional<std::string> asString() const;

  // A missing key, or indexing something that is not an object, yields null
  // so that lookups along a path can be chained.
  [[nodiscard]] const value& operator[](std::string_view key) const;
  // Elements of an array; empty for any other kind.
  [[nodiscard]] const array& items() const;
  [[nodiscard]] const object& members() const;

  // Adds or replaces a key; turns a null into an object first.
  void set(const std::string& key, value item);
  // Appends; turns a null into an array first.
  void push(value item);

private:
  std::variant<std::nullptr_t, bool, double, std::string, array, object> data_;
};

struct member {
  std::string key;
  value item;
};

[[nodiscard]] result<value> parse(std::string_view text);
// Compact unless `indented`, which puts one member per line.
[[nodiscard]] std::string serialize(const value& document, bool indented = false);

}  // namespace catalist::json

#endif  // CATALIST_CORE_JSON_HPP_
