#include "catalist/config/config.hpp"

#include <cctype>
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
    tab "about"    label="Catalist"  icon="󰄛"
    tab "anime"    label="Anime"     icon="󰴙"  show-count=true
    tab "manga"    label="Manga"     icon="󱥬"  show-count=true
    tab "overview" label="Overview"  icon="󰄫"
    tab "sync"     label="Sync"      icon="󰓦"
    tab "settings" label="Settings"  icon="󰒓"
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

  auto gen = kdlDoc.getNode("general");
  if (gen.has_value()) {
    theme = gen->getPropString("theme", theme);
    mouse = gen->getPropBool("mouse", mouse);
    defaultMedia = gen->getPropString("default-media", defaultMedia);
    defaultSort = gen->getPropString("default-sort", defaultSort);
    reverseSort = gen->getPropBool("reverse-sort", reverseSort);

    for (const auto& child : gen->children) {
      if (child.name == "theme")
        theme = child.getArgString(0, theme);
      else if (child.name == "mouse")
        mouse = child.getArg(0).asBool(mouse);
      else if (child.name == "default-media")
        defaultMedia = child.getArgString(0, defaultMedia);
      else if (child.name == "default-sort")
        defaultSort = child.getArgString(0, defaultSort);
      else if (child.name == "reverse-sort")
        reverseSort = child.getArg(0).asBool(reverseSort);
    }
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
  auto mk = [](const std::string& name, kdl::value v) {
    kdl::node n;
    n.name = name;
    n.args.push_back(v);
    return n;
  };
  auto stringValue = [](const std::string& s) {
    kdl::value v;
    v.valueType = kdl::value::type::kString;
    v.stringVal = s;
    return v;
  };
  auto boolValue = [](bool b) {
    kdl::value v;
    v.valueType = kdl::value::type::kBoolean;
    v.boolVal = b;
    return v;
  };

  bool foundGeneral = false;
  for (auto& n : doc.rootNodes) {
    if (n.name == "general") {
      n.children = {mk("theme", stringValue(theme)), mk("mouse", boolValue(mouse)),
                    mk("default-media", stringValue(defaultMedia)),
                    mk("default-sort", stringValue(defaultSort)),
                    mk("reverse-sort", boolValue(reverseSort))};
      n.props.clear();
      foundGeneral = true;
      break;
    }
  }
  if (!foundGeneral) {
    kdl::node gn;
    gn.name = "general";
    gn.children = {mk("theme", stringValue(theme)), mk("mouse", boolValue(mouse)),
                   mk("default-media", stringValue(defaultMedia)),
                   mk("default-sort", stringValue(defaultSort)),
                   mk("reverse-sort", boolValue(reverseSort))};
    doc.rootNodes.insert(doc.rootNodes.begin(), gn);
  }

  return fileSystem.writeFileAtomic(path, doc.serialize());
}

}  // namespace catalist
