#include "catalist/platform/httpClient.hpp"

#include <array>
#include <cstdio>
#include <sstream>

namespace catalist {

namespace {

struct PipeDeleter {
  void operator()(FILE* fp) const {
    if (fp)
      pclose(fp);
  }
};

}  // namespace

Result<HttpResponse> CurlHttpClient::execute(const HttpRequest& request) {
  if (request.url.empty()) {
    return Result<HttpResponse>::failure("HTTP request URL cannot be empty.");
  }

  std::ostringstream cmd;
  cmd << "curl -s -S -L -m " << (request.timeout.count() / 1000) << " -X "
      << request.method;

  for (const auto& [headerKey, headerVal] : request.headers) {
    cmd << " -H \"" << headerKey << ": " << headerVal << "\"";
  }

  cmd << " \"" << request.url << "\" 2>/dev/null";

  std::unique_ptr<FILE, PipeDeleter> pipe;
  if (!request.body.empty()) {
    std::string fullCmd = cmd.str() + " --data-binary @-";
    pipe.reset(popen(fullCmd.c_str(), "w"));
    if (!pipe) {
      return Result<HttpResponse>::failure("Failed to spawn curl subprocess.");
    }
    fwrite(request.body.data(), 1, request.body.size(), pipe.get());
    HttpResponse resp;
    resp.statusCode = 200;
    resp.status_code = 200;
    return Result<HttpResponse>::success(resp);
  }

  pipe.reset(popen(cmd.str().c_str(), "r"));
  if (!pipe) {
    return Result<HttpResponse>::failure("Failed to spawn curl subprocess.");
  }

  std::array<char, 512> buffer;
  std::string responseBody;
  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) !=
         nullptr) {
    responseBody += buffer.data();
  }

  HttpResponse response;
  response.statusCode = responseBody.empty() ? 500 : 200;
  response.status_code = response.statusCode;
  response.body = std::move(responseBody);
  return Result<HttpResponse>::success(response);
}

void MockHttpClient::setResponse(const std::string& urlPattern,
                                 const HttpResponse& response) {
  responses_[urlPattern] = response;
}

Result<HttpResponse> MockHttpClient::execute(const HttpRequest& request) {
  for (const auto& [pattern, resp] : responses_) {
    if (request.url.find(pattern) != std::string::npos) {
      return Result<HttpResponse>::success(resp);
    }
  }
  return Result<HttpResponse>::failure("MockHttpClient: No fixture for " +
                                       request.url);
}

}  // namespace catalist
