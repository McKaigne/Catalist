#include "catalist/tui/tabs/settingsTab.hpp"

#include <algorithm>

#include "catalist/storage/backup.hpp"
#include "catalist/tui/actions.hpp"
#include "catalist/tui/keys.hpp"
#include "catalist/tui/theme/palettes.hpp"
#include "catalist/tui/theme/themeRegistry.hpp"

namespace catalist {

using namespace ftxui;

namespace {

constexpr int kPreferenceRows = 5;
// Data panel rows before the backup list: backup, import, export, sync,
// account.
constexpr int kDataActions = 5;

const std::vector<std::string>& mediaCycle() {
  static const std::vector<std::string> cycle = {"anime", "manga"};
  return cycle;
}

const std::vector<std::string>& sortCycle() {
  static const std::vector<std::string> cycle = {"score", "title", "date",
                                                 "progress"};
  return cycle;
}

std::string nextInCycle(const std::vector<std::string>& cycle,
                        const std::string& current) {
  auto found = std::find(cycle.begin(), cycle.end(), current);
  if (found == cycle.end() || std::next(found) == cycle.end())
    return cycle.front();
  return *std::next(found);
}

std::string checkbox(bool checked, const std::string& label) {
  return (checked ? icon::kCheckSingle : icon::kCheckBlank) + " " + label;
}

Element cursorRow(bool active, const std::string& name, const std::string& value,
                  const themePalette& theme) {
  return hbox({text(active ? " ▌ " : "   ") | color(theme.mauve) | bold,
               text(name) | size(WIDTH, EQUAL, 20) | (active ? bold : nothing) |
                   color(active ? theme.peach : theme.text),
               text(" " + value) | color(theme.lavender) | bold});
}

Element panelCard(const std::string& heading, Color headingColor, Element body,
                  bool focused, const themePalette& theme) {
  return vbox({text(" " + heading) | bold | color(headingColor), text(""),
               body | flex}) |
         border | color(focused ? theme.mauve : theme.surface0);
}

}  // namespace

settingsTab::settingsTab(std::vector<std::string> tabIds)
    : tabIds_(std::move(tabIds)) {}

std::string settingsTab::id() const {
  return "settings";
}

std::string settingsTab::title() const {
  return "Settings";
}

std::string settingsTab::icon() const {
  return "󰒓";
}

void settingsTab::onActivate(appContext& ctx) {
  auto themes = themeRegistry::getAvailableThemes();
  auto applied = std::find(themes.begin(), themes.end(), ctx.config.theme);
  themeCursor_ = applied == themes.end()
                     ? 0
                     : static_cast<int>(applied - themes.begin());
}

void settingsTab::onDeactivate(appContext& ctx) {
  revertPreview(ctx);
  pendingRestore_.reset();
}

void settingsTab::previewThemeUnderCursor() {
  auto themes = themeRegistry::getAvailableThemes();
  if (themeCursor_ >= 0 && themeCursor_ < static_cast<int>(themes.size()))
    themeRegistry::setActive(themes[themeCursor_]);
}

void settingsTab::revertPreview(appContext& ctx) {
  themeRegistry::setActive(ctx.config.theme);
}

void settingsTab::applySelectedTheme(appContext& ctx) {
  auto themes = themeRegistry::getAvailableThemes();
  if (themeCursor_ < 0 || themeCursor_ >= static_cast<int>(themes.size()))
    return;
  ctx.config.theme = themes[themeCursor_];
  themeRegistry::setActive(ctx.config.theme);
  saveConfig(ctx);
  ctx.notices.setStatus("Applied theme: " + ctx.config.theme);
}

void settingsTab::cyclePreference(appContext& ctx) {
  auto& config = ctx.config;
  switch (preferenceCursor_) {
    case 0:
      config.mouse = !config.mouse;
      ctx.notices.setStatus(config.mouse ? "Mouse enabled." : "Mouse disabled.");
      break;
    case 1:
      config.startupTab =
          nextInCycle(tabIds_, config.startupTab.value_or(config.defaultMedia));
      ctx.notices.setStatus("Startup tab: " + *config.startupTab);
      break;
    case 2:
      config.defaultMedia = nextInCycle(mediaCycle(), config.defaultMedia);
      ctx.notices.setStatus("Default media: " + config.defaultMedia);
      break;
    case 3:
      config.defaultSort = nextInCycle(sortCycle(), config.defaultSort);
      ctx.notices.setStatus("Default sort: " + config.defaultSort);
      break;
    default:
      config.reverseSort = !config.reverseSort;
      ctx.notices.setStatus(config.reverseSort ? "Sort: Descending"
                                               : "Sort: Ascending");
      break;
  }
  saveConfig(ctx);
}

void settingsTab::runDataAction(appContext& ctx) {
  if (dataCursor_ == 0) {
    createBackup(ctx);
  } else if (dataCursor_ == 1) {
    importXmlInteractively(ctx);
  } else if (dataCursor_ == 2) {
    exportCatalog(ctx);
  } else if (dataCursor_ == 3) {
    startBackgroundSync(ctx);
  } else if (dataCursor_ == 4) {
    if (ctx.auth.isAuthenticated()) {
      ctx.auth.logout();
      ctx.notices.setStatus("Logged out of MyAnimeList.");
    } else {
      startMalLogin(ctx);
    }
  } else {
    auto snapshots =
        backup::listSnapshots(ctx.fileSystem, ctx.db.getConfigPath());
    pendingRestore_ = snapshots.at(static_cast<size_t>(dataCursor_ - kDataActions));
    ctx.notices.setStatus("Restore " + *pendingRestore_ +
                          "? [ret] confirm  [Esc] cancel");
  }
}

bool settingsTab::handlePendingRestore(const Event& event, appContext& ctx) {
  if (keys::isConfirm(event))
    restoreBackup(ctx, *pendingRestore_);
  else if (event != Event::Escape)
    return true;
  else
    ctx.notices.setStatus("Restore canceled.");
  pendingRestore_.reset();
  return true;
}

int settingsTab::lastRow(appContext& ctx) const {
  if (activePanel_ == panel::kThemes)
    return static_cast<int>(themeRegistry::getAvailableThemes().size()) - 1;
  if (activePanel_ == panel::kPreferences)
    return kPreferenceRows - 1;
  auto snapshots = backup::listSnapshots(ctx.fileSystem, ctx.db.getConfigPath());
  return kDataActions - 1 + static_cast<int>(snapshots.size());
}

void settingsTab::moveCursor(int delta, appContext& ctx) {
  int last = std::max(0, lastRow(ctx));
  int* cursor = activePanel_ == panel::kThemes        ? &themeCursor_
                : activePanel_ == panel::kPreferences ? &preferenceCursor_
                                                      : &dataCursor_;
  *cursor = std::clamp(*cursor + delta, 0, last);
  if (activePanel_ == panel::kThemes)
    previewThemeUnderCursor();
}

bool settingsTab::onEvent(const Event& event, appContext& ctx) {
  if (pendingRestore_)
    return handlePendingRestore(event, ctx);
  const auto& keybinds = ctx.config.keybinds;
  int panelIndex = static_cast<int>(activePanel_);
  if (keys::isMoveLeft(event, keybinds) || keys::isMoveRight(event, keybinds)) {
    int step = keys::isMoveLeft(event, keybinds) ? -1 : 1;
    revertPreview(ctx);
    activePanel_ = static_cast<panel>(std::clamp(panelIndex + step, 0, 2));
    return true;
  }
  if (keys::isMoveDown(event, keybinds) || keys::isMoveUp(event, keybinds)) {
    moveCursor(keys::isMoveDown(event, keybinds) ? 1 : -1, ctx);
    return true;
  }
  if (keys::isJumpTop(event, keybinds) || keys::isJumpBottom(event, keybinds)) {
    moveCursor(keys::isJumpTop(event, keybinds) ? -1000 : 1000, ctx);
    return true;
  }
  if (!keys::isConfirmOrSpace(event))
    return false;
  if (activePanel_ == panel::kThemes)
    applySelectedTheme(ctx);
  else if (activePanel_ == panel::kPreferences)
    cyclePreference(ctx);
  else
    runDataAction(ctx);
  return true;
}

Element settingsTab::renderThemes(appContext& ctx) const {
  const auto& theme = themeRegistry::active();
  bool focused = activePanel_ == panel::kThemes;
  Elements rows;
  auto themes = themeRegistry::getAvailableThemes();
  for (size_t i = 0; i < themes.size(); ++i) {
    bool applied = ctx.config.theme == themes[i];
    bool cursorHere = focused && themeCursor_ == static_cast<int>(i);
    Color nameColor = cursorHere ? theme.peach : (applied ? theme.mauve : theme.text);
    rows.push_back(
        hbox({text(cursorHere ? " ▌ " : "   ") | color(theme.mauve) | bold,
              text((applied ? icon::kCheckSingle : icon::kCheckBlank) + " ") |
                  color(applied ? theme.green : theme.overlay0) | bold,
              text(themes[i]) | (applied ? bold : nothing) | color(nameColor)}));
  }
  auto body = vbox({vbox(std::move(rows)) | vscroll_indicator | frame | flex,
                    text(""),
                    text(" Moving previews; [ret] keeps it.") |
                        color(theme.overlay0)});
  return panelCard("Theme", theme.mauve, body, focused, theme) |
         size(WIDTH, EQUAL, 34);
}

Element settingsTab::renderPreferences(appContext& ctx) const {
  const auto& theme = themeRegistry::active();
  const auto& config = ctx.config;
  bool focused = activePanel_ == panel::kPreferences;
  auto row = [&](int index, const std::string& name, const std::string& value) {
    return cursorRow(focused && preferenceCursor_ == index, name, value, theme);
  };
  auto body = vbox(
      {row(0, "Mouse Support:",
           checkbox(config.mouse, config.mouse ? "Enabled" : "Disabled")),
       row(1, "Startup Tab:", config.startupTab.value_or(config.defaultMedia)),
       row(2, "Default Media:", config.defaultMedia),
       row(3, "Default Sort:", config.defaultSort),
       row(4, "Sort Direction:",
           config.reverseSort ? "Descending ▼" : "Ascending ▲"),
       filler(),
       text(" [ret]/[spc] cycles the value.") | color(theme.overlay0),
       text(" Saved to catalist.kdl") | color(theme.overlay0)});
  return panelCard("Preferences", theme.peach, body, focused, theme) |
         size(WIDTH, EQUAL, 44);
}

Element settingsTab::renderData(appContext& ctx) const {
  const auto& theme = themeRegistry::active();
  bool focused = activePanel_ == panel::kData;
  auto row = [&](int index, const std::string& label) {
    bool active = focused && dataCursor_ == index;
    return hbox({text(active ? " ▌ " : "   ") | color(theme.mauve) | bold,
                 text(label) | (active ? bold : nothing) |
                     color(active ? theme.peach : theme.text)});
  };
  bool loggedIn = ctx.auth.isAuthenticated();
  std::string running = ctx.jobs.runningName();
  Elements rows = {row(0, "󰆓 Create backup snapshot"),
                   row(1, "󰈞 Import MyAnimeList XML"),
                   row(2, "󰈝 Export catalogue as CSV"),
                   row(3, icon::kSync + " Sync ongoing titles with MyAnimeList"),
                   row(4, loggedIn ? "󰍃 Log out of MyAnimeList"
                                   : "󰍂 Log in to MyAnimeList"),
                   text(""),
                   text("   Backups (newest first)") | bold | color(theme.lavender)};
  auto snapshots = backup::listSnapshots(ctx.fileSystem, ctx.db.getConfigPath());
  for (size_t i = 0; i < snapshots.size(); ++i)
    rows.push_back(row(kDataActions + static_cast<int>(i), "  " + snapshots[i]));
  if (snapshots.empty())
    rows.push_back(text("     none yet") | color(theme.overlay0));

  auto body = vbox({vbox(std::move(rows)) | vscroll_indicator | frame | flex,
                    text(""),
                    text(running.empty() ? " [ret] runs; on a backup, restores."
                                         : " Running: " + running + "...") |
                        color(running.empty() ? theme.overlay0 : theme.yellow)});
  return panelCard("Data", theme.green, body, focused, theme) | flex;
}

Element settingsTab::render(appContext& ctx) {
  return hbox({renderThemes(ctx), text(" "), renderPreferences(ctx), text(" "),
               renderData(ctx)});
}

std::vector<hint> settingsTab::hints() const {
  if (pendingRestore_)
    return {{"ret", "restore"}, {"Esc", "cancel"}};
  return {{"h/l", "panel"}, {"j/k", "move"}, {"ret", "apply"}, {"q", "quit"}};
}

}  // namespace catalist
