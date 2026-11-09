#include "catalist/platform/httpClient.hpp"

#include "catalist/core/shell.hpp"

#include <array>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace catalist {

namespace {

struct pipeDeleter {
  void operator()(FILE* fp) const {
    if (fp)
      pclose(fp);
  }
};

}  // namespace

result<httpResponse> curlHttpClient::execute(const httpRequest& request) {
  if (request.url.empty()) {
    return result<httpResponse>::failure("HTTP request URL cannot be empty.");
  }

  std::ostringstream cmd;
  cmd << "curl -s -S -L -m " << (request.timeout.count() / 1000) << " -X "
      << shell::escapeShellArg(request.method);

  for (const auto& [headerKey, headerVal] : request.headers) {
    cmd << " -H " << shell::escapeShellArg(headerKey + ": " + headerVal);
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
      cmd << " --data-binary @" << shell::escapeShellArg(tempBodyFile.string());
    }
  }

  cmd << " -w \"\\nCATALIST_HTTP_STATUS:%{http_code}\" "
      << shell::escapeShellArg(request.url) << " 2>/dev/null";

  std::unique_ptr<FILE, pipeDeleter> pipe(popen(cmd.str().c_str(), "r"));
  if (!tempBodyFile.empty()) {
    std::error_code ec;
    std::filesystem::remove(tempBodyFile, ec);
  }

  if (!pipe) {
    return result<httpResponse>::failure("Failed to spawn curl subprocess.");
  }

  std::array<char, 512> buffer;
  std::string rawOutput;
  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) !=
         nullptr) {
    rawOutput += buffer.data();
  }

  httpResponse response;
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

  return result<httpResponse>::success(response);
}

void mockHttpClient::setResponse(const std::string& urlPattern,
                                 const httpResponse& response) {
  responses_[urlPattern] = response;
}

result<httpResponse> mockHttpClient::execute(const httpRequest& request) {
  requests_.push_back(request);
  for (const auto& [pattern, resp] : responses_) {
    if (request.url.find(pattern) != std::string::npos) {
      return result<httpResponse>::success(resp);
    }
  }
  return result<httpResponse>::failure("MockHttpClient: No fixture for " +
                                       request.url);
}

}  // namespace catalist
