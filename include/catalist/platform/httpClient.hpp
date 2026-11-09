#ifndef CATALIST_PLATFORM_HTTP_CLIENT_HPP_
#define CATALIST_PLATFORM_HTTP_CLIENT_HPP_

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "catalist/core/result.hpp"

namespace catalist {

struct httpRequest {
  std::string url;
  std::string method = "GET";
  std::unordered_map<std::string, std::string> headers;
  std::string body;
  std::chrono::milliseconds timeout{8000};
};

struct httpResponse {
  int statusCode = 0;
  std::string body;
  std::string errorMessage;
};

class iHttpClient {
public:
  virtual ~iHttpClient() = default;
  [[nodiscard]] virtual result<httpResponse> execute(
      const httpRequest& request) = 0;
};

class curlHttpClient : public iHttpClient {
public:
  result<httpResponse> execute(const httpRequest& request) override;
};

// Answers from the responder first (when it returns a response), else from
// the fixture whose URL pattern is the longest substring match.
class mockHttpClient : public iHttpClient {
public:
  using responder =
      std::function<std::optional<httpResponse>(const httpRequest& request)>;

  void setResponse(const std::string& urlPattern, const httpResponse& response);
  void setResponder(responder answer);
  result<httpResponse> execute(const httpRequest& request) override;

  [[nodiscard]] const std::vector<httpRequest>& requests() const {
    return requests_;
  }

private:
  std::unordered_map<std::string, httpResponse> responses_;
  responder responder_;
  std::vector<httpRequest> requests_;
};

}  // namespace catalist

#endif  // CATALIST_PLATFORM_HTTP_CLIENT_HPP_
