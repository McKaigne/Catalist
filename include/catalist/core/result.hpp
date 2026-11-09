#ifndef CATALIST_CORE_RESULT_HPP_
#define CATALIST_CORE_RESULT_HPP_

#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace catalist {

// C++17 stand-in for std::expected. The error type defaults to a message
// string; callers that must branch on the failure use a structured type.
template <typename T, typename E = std::string>
class result {
  static_assert(!std::is_same_v<T, E>,
                "value and error types must differ to stay distinguishable");

public:
  static result success(T value) { return result(std::move(value)); }
  static result failure(E failureValue) {
    return result(error{std::move(failureValue)});
  }

  [[nodiscard]] bool ok() const { return std::holds_alternative<T>(storage_); }

  [[nodiscard]] const T& value() const { return std::get<T>(storage_); }
  [[nodiscard]] T& value() { return std::get<T>(storage_); }

  [[nodiscard]] const E& errorValue() const {
    return std::get<error>(storage_).payload;
  }
  [[nodiscard]] const E& errorMessage() const { return errorValue(); }

private:
  struct error {
    E payload;
  };
  std::variant<T, error> storage_;

  explicit result(T val) : storage_(std::move(val)) {}
  explicit result(error err) : storage_(std::move(err)) {}
};

}  // namespace catalist
#endif  // CATALIST_CORE_RESULT_HPP_
