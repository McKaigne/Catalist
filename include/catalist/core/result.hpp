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

  static result Success(T value) { return success(std::move(value)); }
  static result Failure(std::string message) { return failure(std::move(message)); }

  [[nodiscard]] bool ok() const { return std::holds_alternative<T>(storage_); }
  [[nodiscard]] bool Ok() const { return ok(); }

  [[nodiscard]] const T& value() const { return std::get<T>(storage_); }
  [[nodiscard]] T& value() { return std::get<T>(storage_); }

  [[nodiscard]] const T& Value() const { return value(); }
  [[nodiscard]] T& Value() { return value(); }

  [[nodiscard]] const std::string& errorMessage() const {
    return std::get<error>(storage_).message;
  }
  [[nodiscard]] const std::string& ErrorMessage() const {
    return errorMessage();
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
