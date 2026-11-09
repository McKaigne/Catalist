#ifndef CATALIST_CORE_RESULT_HPP_
#define CATALIST_CORE_RESULT_HPP_

#include <string>
#include <variant>

namespace catalist {

template <typename T>
class result {
public:
  static result success(T value) { return result(std::move(value)); }
  static result failure(std::string message) {
    return result(error{std::move(message)});
  }

  [[nodiscard]] bool ok() const { return std::holds_alternative<T>(storage_); }

  [[nodiscard]] const T& value() const { return std::get<T>(storage_); }
  [[nodiscard]] T& value() { return std::get<T>(storage_); }

  [[nodiscard]] const std::string& errorMessage() const {
    return std::get<error>(storage_).message;
  }

private:
  struct error {
    std::string message;
  };
  std::variant<T, error> storage_;

  explicit result(T val) : storage_(std::move(val)) {}
  explicit result(error err) : storage_(std::move(err)) {}
};

}  // namespace catalist
#endif  // CATALIST_CORE_RESULT_HPP_
