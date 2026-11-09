#include "catalist/cli/commands/commands.hpp"

#include <algorithm>
#include <functional>
#include <iostream>

#include "catalist/cli/args.hpp"
#include "catalist/cli/output.hpp"

namespace catalist::cli {

namespace {

struct settingField {
  const char* name;
  std::function<std::string(const appSettings&)> read;
  // Returns false when the value is not acceptable for this key.
  std::function<bool(appSettings&, const std::string&)> write;
};

std::string flagText(bool flag) {
  return flag ? "true" : "false";
}

bool writeFlag(bool& field, const std::string& text) {
  if (text != "true" && text != "false")
    return false;
  field = text == "true";
  return true;
}

bool writeChoice(std::string& field, const std::vector<std::string>& choices,
                 const std::string& text) {
  if (std::find(choices.begin(), choices.end(), text) == choices.end())
    return false;
  field = text;
  return true;
}

const std::vector<settingField>& fields() {
  static const std::vector<settingField> table = {
      {"mouse", [](const appSettings& s) { return flagText(s.mouse); },
       [](appSettings& s, const std::string& v) { return writeFlag(s.mouse, v); }},
      {"theme", [](const appSettings& s) { return s.theme; },
       [](appSettings& s, const std::string& v) {
         s.theme = v;
         return !v.empty();
       }},
      {"default_media", [](const appSettings& s) { return s.defaultMedia; },
       [](appSettings& s, const std::string& v) {
         return writeChoice(s.defaultMedia, mediaChoices(), v);
       }},
      {"default_sort", [](const appSettings& s) { return s.defaultSort; },
       [](appSettings& s, const std::string& v) {
         return writeChoice(s.defaultSort, sortChoices(), v);
       }},
      {"reverse_sort", [](const appSettings& s) { return flagText(s.reverseSort); },
       [](appSettings& s, const std::string& v) {
         return writeFlag(s.reverseSort, v);
       }},
      {"startup_tab",
       [](const appSettings& s) { return s.startupTab.value_or(s.defaultMedia); },
       [](appSettings& s, const std::string& v) {
         std::string chosen;
         if (!writeChoice(chosen, tabChoices(), v))
           return false;
         s.startupTab = chosen;
         return true;
       }},
  };
  return table;
}

const settingField* findField(const std::string& name) {
  for (const auto& field : fields()) {
    if (name == field.name)
      return &field;
  }
  return nullptr;
}

int listSettings(const appSettings& settings) {
  for (const auto& field : fields())
    std::cout << field.name << " = " << field.read(settings) << "\n";
  return 0;
}

int getSetting(const commandArgs& args, const appSettings& settings) {
  if (args.size() < 2) {
    emitError("usage", "usage: catalist config get <key>", false);
    return 2;
  }
  const settingField* field = findField(args[1]);
  if (!field) {
    emitError("config", "unknown config key '" + args[1] + "'.", false);
    return 1;
  }
  std::cout << field->read(settings) << "\n";
  return 0;
}

int setSetting(const commandArgs& args, commandContext& ctx) {
  if (args.size() < 3) {
    emitError("usage", "usage: catalist config set <key> <value>", false);
    return 2;
  }
  const settingField* field = findField(args[1]);
  if (!field) {
    emitError("config", "unknown config key '" + args[1] + "'.", false);
    return 1;
  }
  if (!field->write(ctx.settings, args[2])) {
    emitError("config", "invalid value '" + args[2] + "' for " + args[1] + ".",
              false);
    return 1;
  }
  if (!saveSettings(ctx.fileSystem, ctx.settingsPath, ctx.settings)) {
    emitError("io", "could not write " + ctx.settingsPath.string() + ".", false);
    return 1;
  }
  std::cout << "Set " << args[1] << " = " << args[2] << "\n";
  return 0;
}

}  // namespace

int runConfig(const commandArgs& args, commandContext& ctx) {
  if (args.empty() || args[0] == "list")
    return listSettings(ctx.settings);
  if (args[0] == "get")
    return getSetting(args, ctx.settings);
  if (args[0] == "set")
    return setSetting(args, ctx);
  emitError("usage",
            "unknown config action '" + args[0] + "'. Expected: list, get, set.",
            false);
  return 2;
}

}  // namespace catalist::cli
