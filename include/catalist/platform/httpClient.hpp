#ifndef CATALIST_PLATFORM_HTTP_CLIENT_HPP_
#define CATALIST_PLATFORM_HTTP_CLIENT_HPP_

#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>

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

  int status_code = 0;
  std::string error_message;
};

class iHttpClient {
public:
  virtual ~iHttpClient() = default;
  [[nodiscard]] virtual result<httpResponse> execute(
      const httpRequest& request) = 0;
  [[nodiscard]] result<httpResponse> Execute(const httpRequest& request) {
    return execute(request);
  }
};

class curlHttpClient : public iHttpClient {
public:
  result<httpResponse> execute(const httpRequest& request) override;
};

class mockHttpClient : public iHttpClient {
public:
  void setResponse(const std::string& urlPattern, const httpResponse& response);
  void SetResponse(const std::string& urlPattern,
                   const httpResponse& response) {
    setResponse(urlPattern, response);
  }
  result<httpResponse> execute(const httpRequest& request) override;

private:
  std::unordered_map<std::string, httpResponse> responses_;
};

}  // namespace catalist

#endif  // CATALIST_PLATFORM_HTTP_CLIENT_HPP_
