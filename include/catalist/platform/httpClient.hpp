#ifndef CATALIST_PLATFORM_HTTP_CLIENT_HPP_
#define CATALIST_PLATFORM_HTTP_CLIENT_HPP_

#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>
#include "catalist/core/result.hpp"

namespace catalist {

struct HttpRequest {
  std::string url;
  std::string method = "GET";
  std::unordered_map<std::string, std::string> headers;
  std::string body;
  std::chrono::milliseconds timeout{8000};
};

struct HttpResponse {
  int statusCode = 0;
  std::string body;
  std::string errorMessage;

  int status_code = 0;
  std::string error_message;
};

class IHttpClient {
 public:
  virtual ~IHttpClient() = default;
  [[nodiscard]] virtual Result<HttpResponse> execute(const HttpRequest& request) = 0;
  [[nodiscard]] Result<HttpResponse> Execute(const HttpRequest& request) { return execute(request); }
};

class CurlHttpClient : public IHttpClient {
 public:
  Result<HttpResponse> execute(const HttpRequest& request) override;
};

class MockHttpClient : public IHttpClient {
 public:
  void setResponse(const std::string& urlPattern, const HttpResponse& response);
  void SetResponse(const std::string& urlPattern, const HttpResponse& response) { setResponse(urlPattern, response); }
  Result<HttpResponse> execute(const HttpRequest& request) override;

 private:
  std::unordered_map<std::string, HttpResponse> responses_;
};

}  // namespace catalist

#endif  // CATALIST_PLATFORM_HTTP_CLIENT_HPP_
