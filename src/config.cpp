#include "catalist/config.hpp"
#include <fstream>
#include <sstream>

namespace catalist {

static const char* DEFAULT_KDL = R"kdl(// ==============================================================================
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
    mode-badge
    separator color="surface1"
    selection-counter
    hints
    spacer
    status-message max-length=36 color="yellow"
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

bool AppConfig::generateDefault(const std::filesystem::path& path) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream file(path);
  if (!file.is_open()) return false;
  file << DEFAULT_KDL;
  file.close();

  std::string err;
  kdlDoc = kdl::Document::parse(DEFAULT_KDL, err);
  return true;
}

bool AppConfig::load(const std::filesystem::path& path) {
  if (!std::filesystem::exists(path)) {
    return generateDefault(path);
  }

  std::ifstream file(path);
  if (!file.is_open()) return false;

  std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  std::string err;
  kdlDoc = kdl::Document::parse(content, err);

  auto gen = kdlDoc.getNode("general");
  if (gen.has_value()) {
    theme = gen->getPropString("theme", theme);
    mouse = gen->getPropBool("mouse", mouse);
    defaultMedia = gen->getPropString("default-media", defaultMedia);
    defaultSort = gen->getPropString("default-sort", defaultSort);
    reverseSort = gen->getPropBool("reverse-sort", reverseSort);

    for (const auto& child : gen->children) {
      if (child.name == "theme") theme = child.getArgString(0, theme);
      else if (child.name == "mouse") mouse = child.getArg(0).asBool(mouse);
      else if (child.name == "default-media") defaultMedia = child.getArgString(0, defaultMedia);
      else if (child.name == "default-sort") defaultSort = child.getArgString(0, defaultSort);
      else if (child.name == "reverse-sort") reverseSort = child.getArg(0).asBool(reverseSort);
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

  ThemeRegistry::setActive(theme);
  return true;
}

bool AppConfig::save(const std::filesystem::path& path) const {
  kdl::Document doc = kdlDoc;
  auto mk = [](const std::string& name, kdl::Value v) { kdl::Node n; n.name = name; n.args.push_back(v); return n; };
  auto S = [](const std::string& s) { kdl::Value v; v.type = kdl::Value::Type::String; v.stringVal = s; return v; };
  auto B = [](bool b) { kdl::Value v; v.type = kdl::Value::Type::Boolean; v.boolVal = b; return v; };

  bool foundGeneral = false;
  for (auto& n : doc.rootNodes) {
    if (n.name == "general") {
      n.children = {
        mk("theme", S(theme)),
        mk("mouse", B(mouse)),
        mk("default-media", S(defaultMedia)),
        mk("default-sort", S(defaultSort)),
        mk("reverse-sort", B(reverseSort))
      };
      n.props.clear();
      foundGeneral = true;
      break;
    }
  }
  if (!foundGeneral) {
    kdl::Node gn;
    gn.name = "general";
    gn.children = {
      mk("theme", S(theme)),
      mk("mouse", B(mouse)),
      mk("default-media", S(defaultMedia)),
      mk("default-sort", S(defaultSort)),
      mk("reverse-sort", B(reverseSort))
    };
    doc.rootNodes.insert(doc.rootNodes.begin(), gn);
  }

  std::filesystem::create_directories(path.parent_path());
  std::ofstream file(path);
  if (!file.is_open()) return false;
  file << doc.serialize();
  return true;
}

} // namespace catalist
