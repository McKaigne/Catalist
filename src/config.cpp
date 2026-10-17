#include "catalist/config.hpp"
#include <fstream>
#include <sstream>

namespace catalist {

bool AppConfig::load(const std::filesystem::path& path) {
  std::ifstream file(path);
  if (!file.is_open()) return false;

  std::string line;
  while (std::getline(file, line)) {
    if (line.empty() || line[0] == '#' || line[0] == '[') continue;
    auto eq = line.find('=');
    if (eq == std::string::npos) continue;

    std::string key = line.substr(0, eq);
    std::string val = line.substr(eq + 1);

    auto trim = [](std::string& s) {
      s.erase(0, s.find_first_not_of(" \t\r\n\""));
      s.erase(s.find_last_not_of(" \t\r\n\"") + 1);
    };
    trim(key);
    trim(val);

    if (key == "mouse") mouse = (val == "true" || val == "1");
    else if (key == "theme") theme = val;
    else if (key == "default_media" || key == "defaultMedia") defaultMedia = val;
    else if (key == "default_sort" || key == "defaultSort") defaultSort = val;
    else if (key == "reverse_sort" || key == "reverseSort") reverseSort = (val == "true" || val == "1");
  }
  return true;
}

bool AppConfig::save(const std::filesystem::path& path) const {
  std::ofstream file(path);
  if (!file.is_open()) return false;

  file << "# Catalist Configuration\n"
       << "[general]\n"
       << "mouse = " << (mouse ? "true" : "false") << "\n"
       << "theme = \"" << theme << "\"\n"
       << "defaultMedia = \"" << defaultMedia << "\"\n\n"
       << "[sorting]\n"
       << "defaultSort = \"" << defaultSort << "\"\n"
       << "reverseSort = " << (reverseSort ? "true" : "false") << "\n";
  return true;
}

} // namespace catalist
