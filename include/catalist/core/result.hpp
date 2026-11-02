#ifndef CATALIST_CORE_RESULT_HPP_
#define CATALIST_CORE_RESULT_HPP_

#include <string>
#include <variant>

namespace catalist {

template <typename T>
class Result {
 public:
  static Result success(T value) { return Result(std::move(value)); }
  static Result failure(std::string error) { return Result(Error{std::move(error)}); }

  static Result Success(T value) { return success(std::move(value)); }
  static Result Failure(std::string error) { return failure(std::move(error)); }

  [[nodiscard]] bool ok() const { return std::holds_alternative<T>(storage_); }
  [[nodiscard]] bool Ok() const { return ok(); }

  [[nodiscard]] const T& value() const { return std::get<T>(storage_); }
  [[nodiscard]] T& value() { return std::get<T>(storage_); }

  [[nodiscard]] const T& Value() const { return value(); }
  [[nodiscard]] T& Value() { return value(); }

  [[nodiscard]] const std::string& errorMessage() const { return std::get<Error>(storage_).message; }
  [[nodiscard]] const std::string& ErrorMessage() const { return errorMessage(); }

 private:
  struct Error { std::string message; };
  std::variant<T, Error> storage_;

  explicit Result(T val) : storage_(std::move(val)) {}
  explicit Result(Error err) : storage_(std::move(err)) {}
};

}  // namespace catalist
#endif  // CATALIST_CORE_RESULT_HPP_
