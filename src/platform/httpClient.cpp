#include "catalist/platform/httpClient.hpp"

#include <array>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace catalist {

namespace {

struct PipeDeleter {
  void operator()(FILE* fp) const {
    if (fp)
      pclose(fp);
  }
};

std::string escapeShellArg(std::string_view arg) {
  std::string out = "'";
  for (char c : arg) {
    if (c == '\'') {
      out += "'\\''";
    } else {
      out += c;
    }
  }
  out += "'";
  return out;
}

}  // namespace

Result<HttpResponse> CurlHttpClient::execute(const HttpRequest& request) {
  if (request.url.empty()) {
    return Result<HttpResponse>::failure("HTTP request URL cannot be empty.");
  }

  std::ostringstream cmd;
  cmd << "curl -s -S -L -m " << (request.timeout.count() / 1000) << " -X "
      << escapeShellArg(request.method);

  for (const auto& [headerKey, headerVal] : request.headers) {
    cmd << " -H " << escapeShellArg(headerKey + ": " + headerVal);
  }

  std::filesystem::path tempBodyFile;
  if (!request.body.empty()) {
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    tempBodyFile = std::filesystem::temp_directory_path() /
                   ("catalist_body_" + std::to_string(now) + ".tmp");
    std::ofstream ofs(tempBodyFile, std::ios::binary);
    if (ofs.is_open()) {
      ofs.write(request.body.data(), static_cast<std::streamsize>(request.body.size()));
      ofs.close();
      cmd << " --data-binary @" << escapeShellArg(tempBodyFile.string());
    }
  }

  cmd << " -w \"\\nCATALIST_HTTP_STATUS:%{http_code}\" "
      << escapeShellArg(request.url) << " 2>/dev/null";

  std::unique_ptr<FILE, PipeDeleter> pipe(popen(cmd.str().c_str(), "r"));
  if (!tempBodyFile.empty()) {
    std::error_code ec;
    std::filesystem::remove(tempBodyFile, ec);
  }

  if (!pipe) {
    return Result<HttpResponse>::failure("Failed to spawn curl subprocess.");
  }

  std::array<char, 512> buffer;
  std::string rawOutput;
  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) !=
         nullptr) {
    rawOutput += buffer.data();
  }

  HttpResponse response;
  const std::string marker = "\nCATALIST_HTTP_STATUS:";
  size_t markerPos = rawOutput.rfind(marker);

  if (markerPos != std::string::npos) {
    response.body = rawOutput.substr(0, markerPos);
    std::string codeStr = rawOutput.substr(markerPos + marker.size());
    try {
      response.statusCode = std::stoi(codeStr);
    } catch (...) {
      response.statusCode = response.body.empty() ? 500 : 200;
    }
  } else {
    response.body = std::move(rawOutput);
    response.statusCode = response.body.empty() ? 500 : 200;
  }

  response.status_code = response.statusCode;
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
