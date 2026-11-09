#include "catalist/platform/httpClient.hpp"

#include "catalist/core/shell.hpp"

#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <sstream>

#include <unistd.h>

namespace catalist {

namespace {

// A mkstemp file (mode 0600) removed when the request is done.
class privateTempFile {
public:
  privateTempFile() = default;
  privateTempFile(const privateTempFile&) = delete;
  privateTempFile& operator=(const privateTempFile&) = delete;
  ~privateTempFile() {
    if (!path_.empty())
      ::unlink(path_.c_str());
  }

  bool write(const std::string& content) {
    std::string pattern =
        (std::filesystem::temp_directory_path() / "catalist_XXXXXX").string();
    int descriptor = ::mkstemp(pattern.data());
    if (descriptor < 0)
      return false;
    path_ = pattern;
    size_t offset = 0;
    while (offset < content.size()) {
      ssize_t written =
          ::write(descriptor, content.data() + offset, content.size() - offset);
      if (written <= 0)
        break;
      offset += static_cast<size_t>(written);
    }
    ::close(descriptor);
    return offset == content.size();
  }

  [[nodiscard]] const std::string& path() const { return path_; }

private:
  std::string path_;
};

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

  // Headers carry bearer tokens and bodies carry codes, so both go through
  // owner-only files instead of the command line other users can read.
  privateTempFile headerFile;
  if (!request.headers.empty()) {
    std::string headerLines;
    for (const auto& [headerKey, headerVal] : request.headers)
      headerLines += headerKey + ": " + headerVal + "\n";
    if (!headerFile.write(headerLines))
      return result<httpResponse>::failure("Could not write request headers.");
    cmd << " -H @" << shell::escapeShellArg(headerFile.path());
  }

  privateTempFile bodyFile;
  if (!request.body.empty()) {
    if (!bodyFile.write(request.body))
      return result<httpResponse>::failure("Could not write request body.");
    cmd << " --data-binary @" << shell::escapeShellArg(bodyFile.path());
  }

  cmd << " -w \"\\nCATALIST_HTTP_STATUS:%{http_code}\" "
      << shell::escapeShellArg(request.url) << " 2>/dev/null";

  std::unique_ptr<FILE, pipeDeleter> pipe(popen(cmd.str().c_str(), "r"));
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

void mockHttpClient::setResponder(responder answer) {
  responder_ = std::move(answer);
}

result<httpResponse> mockHttpClient::execute(const httpRequest& request) {
  requests_.push_back(request);
  if (responder_) {
    if (auto answered = responder_(request))
      return result<httpResponse>::success(*answered);
  }
  const httpResponse* best = nullptr;
  size_t bestLength = 0;
  for (const auto& [pattern, resp] : responses_) {
    bool matches = request.url.find(pattern) != std::string::npos;
    if (matches && (best == nullptr || pattern.size() > bestLength)) {
      best = &resp;
      bestLength = pattern.size();
    }
  }
  if (best)
    return result<httpResponse>::success(*best);
  return result<httpResponse>::failure("MockHttpClient: No fixture for " +
                                       request.url);
}

}  // namespace catalist
