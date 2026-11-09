#include "catalist/settings/settings.hpp"

#include <algorithm>

#include "catalist/core/json.hpp"

namespace catalist {

namespace {

bool isOneOf(const std::vector<std::string>& choices, const std::string& text) {
  return std::find(choices.begin(), choices.end(), text) != choices.end();
}

void readChoice(const json::value& document, const char* key,
                const std::vector<std::string>& choices, std::string& field) {
  auto text = document[key].asString();
  if (text && isOneOf(choices, *text))
    field = *text;
}

void readText(const json::value& document, const char* key,
              std::string& field) {
  auto text = document[key].asString();
  if (text && !text->empty())
    field = *text;
}

void readFlag(const json::value& document, const char* key, bool& field) {
  if (auto flag = document[key].asBool())
    field = *flag;
}

}  // namespace

const std::vector<std::string>& mediaChoices() {
  static const std::vector<std::string> choices = {"anime", "manga"};
  return choices;
}

const std::vector<std::string>& sortChoices() {
  static const std::vector<std::string> choices = {"score", "title", "date",
                                                   "progress"};
  return choices;
}

const std::vector<std::string>& tabChoices() {
  static const std::vector<std::string> choices = {"anime", "manga", "stats",
                                                   "settings"};
  return choices;
}

appSettings loadSettings(const iFileSystem& fileSystem,
                         const std::filesystem::path& path) {
  appSettings settings;
  auto content = fileSystem.readFile(path);
  if (!content)
    return settings;
  auto parsed = json::parse(*content);
  if (!parsed.ok())
    return settings;
  const json::value& document = parsed.value();
  readFlag(document, "mouse", settings.mouse);
  readText(document, "theme", settings.theme);
  readChoice(document, "defaultMedia", mediaChoices(), settings.defaultMedia);
  readChoice(document, "defaultSort", sortChoices(), settings.defaultSort);
  readFlag(document, "reverseSort", settings.reverseSort);
  if (auto tab = document["startupTab"].asString())
    settings.startupTab = *tab;
  readFlag(document, "welcomeShown", settings.welcomeShown);
  return settings;
}

bool saveSettings(iFileSystem& fileSystem, const std::filesystem::path& path,
                  const appSettings& settings) {
  json::value document;
  document.set("mouse", settings.mouse);
  document.set("theme", settings.theme);
  document.set("defaultMedia", settings.defaultMedia);
  document.set("defaultSort", settings.defaultSort);
  document.set("reverseSort", settings.reverseSort);
  if (settings.startupTab)
    document.set("startupTab", *settings.startupTab);
  document.set("welcomeShown", settings.welcomeShown);
  return fileSystem.writeFileAtomic(path, json::serialize(document, true) + "\n");
}

}  // namespace catalist
