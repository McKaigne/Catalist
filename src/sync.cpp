#include "catalist/sync.hpp"
#include <cstdlib>
#include <array>
#include <memory>
#include <regex>

namespace catalist {

struct PipeDeleter {
  void operator()(FILE* f) const {
    if (f) pclose(f);
  }
};

static std::string execCommand(const std::string& cmd) {
  std::array<char, 128> buffer;
  std::string result;
  std::unique_ptr<FILE, PipeDeleter> pipe(popen(cmd.c_str(), "r"));
  if (!pipe) return "";
  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr) {
    result += buffer.data();
  }
  return result;
}

size_t MetadataSync::syncOngoing(Database& db, std::string& outMsg) {
  size_t updated = 0;

  for (auto e : db.getEntries(MediaType::Anime)) {
    if (e.totalUnits == 0 && e.malId > 0) {
      std::string cmd = "curl -s -m 3 \"https://api.jikan.moe/v4/anime/" + std::to_string(e.malId) + "\" 2>/dev/null";
      std::string resp = execCommand(cmd);
      if (resp.empty()) continue;

      std::regex epRe(R"("episodes":\s*(\d+))");
      std::smatch m;
      if (std::regex_search(resp, m, epRe)) {
        try {
          int eps = std::stoi(m[1].str());
          if (eps > 0 && eps != e.totalUnits) {
            e.totalUnits = eps;
            db.updateEntry(e);
            updated++;
          }
        } catch (...) {}
      }
    }
  }

  if (updated > 0) {
    db.saveAll();
    outMsg = "Reconciled " + std::to_string(updated) + " finished records from upstream MAL.";
  } else {
    outMsg = "All records synchronized with database.";
  }

  return updated;
}

} // namespace catalist
