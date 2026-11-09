#include "catalist/config.hpp"

#include <fstream>
#include <sstream>

namespace catalist {

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
    text "v0.2.0" color="overlay0"
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

bool appConfig::generateDefault(const std::filesystem::path& path) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream file(path);
  if (!file.is_open())
    return false;
  file << kDefaultKdl;
  file.close();

  std::string err;
  kdlDoc = kdl::document::parse(kDefaultKdl, err);
  return true;
}

bool appConfig::load(const std::filesystem::path& path) {
  if (!std::filesystem::exists(path)) {
    return generateDefault(path);
  }

  std::ifstream file(path);
  if (!file.is_open())
    return false;

  std::string content((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());
  std::string err;
  kdlDoc = kdl::document::parse(content, err);

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

  themeRegistry::setActive(theme);
  return true;
}

bool appConfig::save(const std::filesystem::path& path) const {
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

  std::filesystem::create_directories(path.parent_path());
  std::ofstream file(path);
  if (!file.is_open())
    return false;
  file << doc.serialize();
  return true;
}

}  // namespace catalist
