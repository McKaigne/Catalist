#include "catalist/tui/tabs/settingsTab.hpp"

#include "catalist/tui/keys.hpp"
#include "catalist/tui/theme/palettes.hpp"
#include "catalist/tui/theme/themeRegistry.hpp"

namespace catalist {

using namespace ftxui;

namespace {

constexpr int kLastPreference = 4;

const std::vector<std::string>& defaultViewCycle() {
  static const std::vector<std::string> cycle = {"anime", "manga", "overview"};
  return cycle;
}

const std::vector<std::string>& defaultSortCycle() {
  static const std::vector<std::string> cycle = {"score", "title", "date",
                                                 "progress"};
  return cycle;
}

std::string nextInCycle(const std::vector<std::string>& cycle,
                        const std::string& current) {
  for (size_t i = 0; i + 1 < cycle.size(); ++i) {
    if (cycle[i] == current)
      return cycle[i + 1];
  }
  return cycle.front();
}

std::string checkbox(bool checked, const std::string& label) {
  return (checked ? icon::kCheckSingle : icon::kCheckBlank) + " " + label;
}

}  // namespace

std::string settingsTab::id() const {
  return "settings";
}

std::string settingsTab::title() const {
  return "Settings";
}

std::string settingsTab::icon() const {
  return "󰒓";
}

void settingsTab::applySelectedTheme(appContext& ctx) {
  auto themes = themeRegistry::getAvailableThemes();
  if (themeCursor_ < 0 || themeCursor_ >= static_cast<int>(themes.size()))
    return;
  const std::string& chosen = themes[themeCursor_];
  ctx.config.theme = chosen;
  themeRegistry::setActive(chosen);
  saveConfig(ctx);
  ctx.notices.setStatus("Applied theme: " + chosen);
}

void settingsTab::cyclePreference(appContext& ctx) {
  auto& config = ctx.config;
  switch (preferenceCursor_) {
    case 0:
      config.mouse = !config.mouse;
      ctx.notices.setStatus(config.mouse ? "Mouse enabled." : "Mouse disabled.");
      break;
    case 1:
      config.defaultMedia = nextInCycle(defaultViewCycle(), config.defaultMedia);
      ctx.notices.setStatus("Default view: " + config.defaultMedia);
      break;
    case 2:
      config.defaultSort = nextInCycle(defaultSortCycle(), config.defaultSort);
      ctx.notices.setStatus("Default sort: " + config.defaultSort);
      break;
    case 3:
      config.reverseSort = false;
      ctx.notices.setStatus("Sort: Ascending");
      break;
    default:
      config.reverseSort = true;
      ctx.notices.setStatus("Sort: Descending");
      break;
  }
  saveConfig(ctx);
}

Element settingsTab::render(appContext& ctx) {
  const auto& theme = themeRegistry::active();
  const auto& config = ctx.config;
  auto themes = themeRegistry::getAvailableThemes();

  Elements themeRows;
  for (size_t i = 0; i < themes.size(); ++i) {
    bool applied = config.theme == themes[i];
    bool cursorHere = activePanel_ == panel::kThemes &&
                      themeCursor_ == static_cast<int>(i);
    Color nameColor =
        cursorHere ? theme.peach : (applied ? theme.mauve : theme.text);
    themeRows.push_back(
        hbox({text(cursorHere ? " ▌ " : "   ") | color(theme.mauve) | bold,
              text((applied ? icon::kCheckSingle : icon::kCheckBlank) + " ") |
                  color(applied ? theme.green : theme.overlay0) | bold,
              text(themes[i]) | (applied ? bold : nothing) | color(nameColor)}));
  }

  auto preferenceRow = [&](int index, const std::string& name,
                           const std::string& value) {
    bool active = activePanel_ == panel::kPreferences &&
                  preferenceCursor_ == index;
    return hbox({text(active ? " ▌ " : "   ") | color(theme.mauve) | bold,
                 text(name) | size(WIDTH, EQUAL, 22) |
                     (active ? bold : nothing) |
                     color(active ? theme.peach : theme.text),
                 text(" " + value) | color(theme.lavender) | bold});
  };
  Color panelBorder[] = {theme.surface0, theme.surface0};
  panelBorder[static_cast<int>(activePanel_)] = theme.mauve;

  auto themesPanel =
      vbox({text(" Visual Theme Palette") | bold | color(theme.mauve), text(""),
            vbox(std::move(themeRows)) | vscroll_indicator | frame | flex,
            text(""),
            text(" Press [ret] to apply selected theme.") |
                color(theme.overlay0)}) |
      border | color(panelBorder[0]) | size(WIDTH, EQUAL, 38);

  auto preferencesPanel =
      vbox({text(" Runtime Preferences") | bold | color(theme.peach), text(""),
            preferenceRow(0, "Mouse Support:",
                          checkbox(config.mouse,
                                   config.mouse ? "Enabled" : "Disabled")),
            text(""), preferenceRow(1, "Default View:", config.defaultMedia),
            text(""), preferenceRow(2, "Default Sort:", config.defaultSort),
            text(""),
            preferenceRow(3, "Sort Descending:",
                          checkbox(config.reverseSort, "High to Low")),
            preferenceRow(4, "Sort Ascending: ",
                          checkbox(!config.reverseSort, "Low to High")),
            text(""),
            text(" Press [ret] or [spc] to cycle options.") |
                color(theme.overlay0),
            text(""),
            text(" Configuration Store:") | bold | color(theme.lavender),
            text(" ~/.config/catalist/catalist.kdl") | color(theme.subtext),
            filler()}) |
      border | color(panelBorder[1]) | flex;

  return vbox({text(""),
               hbox({themesPanel, text(" "), preferencesPanel}) |
                   size(WIDTH, EQUAL, 92) | center | flex,
               text("")}) |
         center | flex;
}

bool settingsTab::onEvent(const Event& event, appContext& ctx) {
  const auto& keybinds = ctx.config.keybinds;
  bool onThemes = activePanel_ == panel::kThemes;
  int lastTheme =
      static_cast<int>(themeRegistry::getAvailableThemes().size()) - 1;
  if (keys::isMoveLeft(event, keybinds)) {
    activePanel_ = panel::kThemes;
    return true;
  }
  if (keys::isMoveRight(event, keybinds)) {
    activePanel_ = panel::kPreferences;
    return true;
  }
  if (keys::isMoveDown(event, keybinds)) {
    if (onThemes)
      themeCursor_ = std::min(themeCursor_ + 1, std::max(0, lastTheme));
    else
      preferenceCursor_ = std::min(preferenceCursor_ + 1, kLastPreference);
    return true;
  }
  if (keys::isMoveUp(event, keybinds)) {
    if (onThemes)
      themeCursor_ = std::max(themeCursor_ - 1, 0);
    else
      preferenceCursor_ = std::max(preferenceCursor_ - 1, 0);
    return true;
  }
  if (!keys::isConfirmOrSpace(event))
    return false;
  if (onThemes)
    applySelectedTheme(ctx);
  else
    cyclePreference(ctx);
  return true;
}

std::vector<hint> settingsTab::hints() const {
  return {{"h/l", "panel"}, {"j/k", "move"}, {"ret", "apply"}, {"q", "quit"}};
}

}  // namespace catalist
