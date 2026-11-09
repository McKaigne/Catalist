#include "catalist/config/config.hpp"

#include <algorithm>
#include <cctype>
#include <set>
#include <sstream>

namespace catalist {

namespace {

bool isDigits(const std::string& text) {
  if (text.empty())
    return false;
  for (char c : text) {
    if (!std::isdigit(static_cast<unsigned char>(c)))
      return false;
  }
  return true;
}

bool isLegacyVersionLabel(const std::string& label) {
  if (label.size() < 2 || label[0] != 'v')
    return false;
  std::stringstream parts(label.substr(1));
  std::string part;
  int partCount = 0;
  while (std::getline(parts, part, '.')) {
    if (!isDigits(part))
      return false;
    ++partCount;
  }
  return partCount == 3;
}

// Older configs hard-coded the release as a `text "vX.Y.Z"` node in
// the top bar; turning those into `version` nodes makes old files show the
// running version instead of a stale string.
void upgradeLegacyVersionNodes(kdl::document& doc) {
  for (auto& root : doc.rootNodes) {
    if (root.name != "top-bar")
      continue;
    for (auto& child : root.children) {
      if (child.name != "text" || !isLegacyVersionLabel(child.getArgString(0)))
        continue;
      child.name = "version";
      child.args.clear();
    }
  }
}

kdl::value stringValue(const std::string& text) {
  kdl::value converted;
  converted.valueType = kdl::value::type::kString;
  converted.stringVal = text;
  return converted;
}

kdl::value boolValue(bool flag) {
  kdl::value converted;
  converted.valueType = kdl::value::type::kBoolean;
  converted.boolVal = flag;
  return converted;
}

kdl::node singleArgNode(const std::string& name, kdl::value argument) {
  kdl::node made;
  made.name = name;
  made.args.push_back(std::move(argument));
  return made;
}

kdl::node tabNode(const std::string& id, const std::string& label,
                  const std::string& glyph, bool showCount) {
  kdl::node tab = singleArgNode("tab", stringValue(id));
  tab.props["label"] = stringValue(label);
  tab.props["icon"] = stringValue(glyph);
  if (showCount)
    tab.props["show-count"] = boolValue(true);
  return tab;
}

std::vector<kdl::node> defaultTabNodes() {
  return {tabNode("anime", "Anime", "󰴙", true),
          tabNode("manga", "Manga", "󱥬", true),
          tabNode("stats", "Stats", "󰄫", false),
          tabNode("settings", "Settings", "󰒓", false)};
}

bool isLegacyTabId(const std::string& id) {
  static const std::set<std::string> removed = {
      "about", "overview", "sync",    "queue",
      "search", "library", "history", "help"};
  return removed.count(id) > 0;
}

bool listsLegacyTabs(const kdl::node& topBar) {
  return std::any_of(topBar.children.begin(), topBar.children.end(),
                     [](const kdl::node& child) {
                       return child.name == "tab" &&
                              isLegacyTabId(child.getArgString(0));
                     });
}

// Top bars written by older versions list tabs that no longer exist; they
// get the current tab set while keeping their spacer, text and version items.
void upgradeLegacyTopBar(kdl::document& doc) {
  for (auto& root : doc.rootNodes) {
    if (root.name != "top-bar" || !listsLegacyTabs(root))
      continue;
    std::vector<kdl::node> upgraded = defaultTabNodes();
    for (const auto& child : root.children) {
      if (child.name != "tab")
        upgraded.push_back(child);
    }
    root.children = std::move(upgraded);
  }
}

void applyGeneralSetting(appConfig& config, const std::string& name,
                         const kdl::value& setting) {
  if (setting.valueType == kdl::value::type::kNull)
    return;
  if (name == "theme")
    config.theme = setting.asString();
  else if (name == "mouse")
    config.mouse = setting.asBool(config.mouse);
  else if (name == "default-media")
    config.defaultMedia = setting.asString();
  else if (name == "default-sort")
    config.defaultSort = setting.asString();
  else if (name == "reverse-sort")
    config.reverseSort = setting.asBool(config.reverseSort);
  else if (name == "startup-tab")
    config.startupTab = setting.asString();
  else if (name == "welcome-shown")
    config.welcomeShown = setting.asBool(config.welcomeShown);
}

std::vector<kdl::node> generalNodes(const appConfig& config) {
  std::vector<kdl::node> nodes = {
      singleArgNode("theme", stringValue(config.theme)),
      singleArgNode("mouse", boolValue(config.mouse)),
      singleArgNode("default-media", stringValue(config.defaultMedia)),
      singleArgNode("default-sort", stringValue(config.defaultSort)),
      singleArgNode("reverse-sort", boolValue(config.reverseSort))};
  if (config.startupTab)
    nodes.push_back(singleArgNode("startup-tab", stringValue(*config.startupTab)));
  nodes.push_back(singleArgNode("welcome-shown", boolValue(config.welcomeShown)));
  return nodes;
}

}  // namespace

static const char* kDefaultKdl =
    R"kdl(// ==============================================================================
// Catalist Workspace Specification (KDL 1.0)
// ==============================================================================

general {
    theme "catppuccin-mocha"
    mouse true
    default-media "anime"
    default-sort "score"
    reverse-sort true
    startup-tab "anime"
}

keybinds {
    move-up "k"
    move-down "j"
    focus-left "h"
    focus-right "l"
    increment "+"
    decrement "-"
    cycle-status "Space"
    add "a"
    delete "d"
    quit "q"
}

top-bar height=3 border=true {
    tab "anime"    label="Anime"    icon="󰴙"  show-count=true
    tab "manga"    label="Manga"    icon="󱥬"  show-count=true
    tab "stats"    label="Stats"    icon="󰄫"
    tab "settings" label="Settings" icon="󰒓"
    spacer
    version color="overlay0"
}

bottom-bar height=3 border=true {
    account-badge
    separator color="surface1"
    selection-counter
    status-message max-length=36 color="yellow"
    spacer
    hints
}

card "details" width=42 border=true {
    category "DETAILS" {
        text "{title}" bold=true color="mauve" marquee=true
        row {
            pill "{type}" fg="crust" bg="{type_color}"
            spacer
            text "ID #{id}" color="subtext"
        }
        meta-row label="Source:" value="{source_display}"
        meta-row label="Dates:" value="{dates_display}"
    }

    category "STATUS" {
        field "status" label="Status:"
    }

    category "METRICS" {
        field "rating" label="Rating:" style="stars"
        field "progress" label="Progress:" style="gauge"
    }

    category "NOTES" {
        field "notes" label="Notes:"
    }
}
)kdl";

bool appConfig::generateDefault(iFileSystem& fileSystem,
                                const std::filesystem::path& path) {
  if (!fileSystem.writeFileAtomic(path, kDefaultKdl))
    return false;

  std::string err;
  kdlDoc = kdl::document::parse(kDefaultKdl, err);
  return true;
}

bool appConfig::load(iFileSystem& fileSystem,
                     const std::filesystem::path& path) {
  if (!fileSystem.exists(path))
    return generateDefault(fileSystem, path);

  auto content = fileSystem.readFile(path);
  if (!content)
    return false;

  std::string err;
  kdlDoc = kdl::document::parse(*content, err);
  upgradeLegacyVersionNodes(kdlDoc);

  upgradeLegacyTopBar(kdlDoc);

  if (auto general = kdlDoc.getNode("general")) {
    for (const auto& [name, setting] : general->props)
      applyGeneralSetting(*this, name, setting);
    for (const auto& child : general->children)
      applyGeneralSetting(*this, child.name, child.getArg(0));
  }

  auto binds = kdlDoc.getNode("keybinds");
  if (binds.has_value()) {
    for (const auto& child : binds->children) {
      keybinds.setBinding(child.name, child.getArgString(0));
    }
    for (const auto& [k, v] : binds->props) {
      keybinds.setBinding(k, v.asString());
    }
  }

  return true;
}

bool appConfig::save(iFileSystem& fileSystem,
                     const std::filesystem::path& path) const {
  kdl::document doc = kdlDoc;
  auto general = std::find_if(doc.rootNodes.begin(), doc.rootNodes.end(),
                              [](const kdl::node& root) {
                                return root.name == "general";
                              });
  if (general == doc.rootNodes.end()) {
    kdl::node created;
    created.name = "general";
    general = doc.rootNodes.insert(doc.rootNodes.begin(), created);
  }
  general->children = generalNodes(*this);
  general->props.clear();
  return fileSystem.writeFileAtomic(path, doc.serialize());
}

}  // namespace catalist
