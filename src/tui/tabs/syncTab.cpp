#include "catalist/tui/tabs/syncTab.hpp"

#include "catalist/storage/backup.hpp"
#include "catalist/tui/actions.hpp"
#include "catalist/tui/keys.hpp"
#include "catalist/tui/theme/themeRegistry.hpp"

namespace catalist {

using namespace ftxui;

namespace {

constexpr int kLastAction = 2;
constexpr size_t kVisibleBackups = 8;

Element cursorMark(bool active, const themePalette& theme) {
  return text(active ? " ▌ " : "   ") | color(theme.mauve) | bold;
}

}  // namespace

std::string syncTab::id() const {
  return "sync";
}

std::string syncTab::title() const {
  return "Sync";
}

std::string syncTab::icon() const {
  return "󰓦";
}

Element syncTab::render(appContext& ctx) {
  const auto& theme = themeRegistry::active();
  auto snapshots = backup::listSnapshots(ctx.fileSystem, ctx.db.getConfigPath());

  Elements backupRows;
  if (snapshots.empty())
    backupRows.push_back(text("  No snapshots found in backups/.") |
                         color(theme.subtext));
  for (size_t i = 0; i < snapshots.size() && i < kVisibleBackups; ++i) {
    bool selected = activePanel_ == panel::kBackups &&
                    backupCursor_ == static_cast<int>(i);
    backupRows.push_back(
        hbox({cursorMark(selected, theme),
              text(snapshots[i]) | (selected ? bold : nothing) |
                  color(selected ? theme.peach : theme.text)}));
  }

  auto actionButton = [&](int index, const std::string& glyph,
                          const std::string& name, const std::string& detail) {
    bool active = activePanel_ == panel::kActions && actionCursor_ == index;
    return vbox({hbox({cursorMark(active, theme),
                       text(glyph + " " + name) | (active ? bold : nothing) |
                           color(active ? theme.peach : theme.text)}),
                 text("     " + detail) | color(theme.subtext), text("")});
  };

  bool syncing = ctx.sync.isRunning();
  std::string syncState =
      syncing ? "● Sync active in background..." : "● Upstream idle and ready.";
  Color panelBorder[] = {theme.surface0, theme.surface0};
  panelBorder[static_cast<int>(activePanel_)] = theme.mauve;

  auto actions =
      vbox({text(" Upstream Operations") | bold | color(theme.lavender),
            text(""),
            text(" Status: " + syncState) | bold |
                color(syncing ? theme.yellow : theme.green),
            text(""),
            actionButton(0, "󰓦", "Reconcile Ongoing Titles",
                         "Fetches finished totals for ongoing anime from MAL."),
            actionButton(1, "󰆓", "Create Snapshot Backup",
                         "Generates an instant timestamped CSV backup replica."),
            actionButton(2, "󰈞", "Ingest MyAnimeList Archive",
                         "Import an exported official myanimelist.xml file."),
            filler(),
            text(" Press [ret] to trigger selected action.") |
                color(theme.overlay0)}) |
      border | color(panelBorder[0]) | size(WIDTH, EQUAL, 44);

  auto backups =
      vbox({text(" Snapshot Backups Repository") | bold | color(theme.peach),
            text(""),
            text(" Location: ~/.config/catalist/backups/") |
                color(theme.subtext),
            text(""),
            vbox(std::move(backupRows)) | vscroll_indicator | frame | flex,
            text(""),
            text(" Press [h/l] to switch panels  │  [j/k] to navigate.") |
                color(theme.overlay0)}) |
      border | color(panelBorder[1]) | flex;

  return vbox({text(""),
               hbox({actions, text(" "), backups}) | size(WIDTH, EQUAL, 92) |
                   center | flex,
               text("")}) |
         center | flex;
}

void syncTab::runSelectedAction(appContext& ctx) {
  if (actionCursor_ == 0)
    startBackgroundSync(ctx);
  else if (actionCursor_ == 1)
    createBackup(ctx);
  else
    importXmlInteractively(ctx);
}

bool syncTab::onEvent(const Event& event, appContext& ctx) {
  const auto& keybinds = ctx.config.keybinds;
  bool onActions = activePanel_ == panel::kActions;
  if (keys::isMoveLeft(event, keybinds)) {
    activePanel_ = panel::kActions;
    return true;
  }
  if (keys::isMoveRight(event, keybinds)) {
    activePanel_ = panel::kBackups;
    return true;
  }
  if (keys::isMoveDown(event, keybinds)) {
    if (onActions)
      actionCursor_ = std::min(actionCursor_ + 1, kLastAction);
    else
      backupCursor_++;
    return true;
  }
  if (keys::isMoveUp(event, keybinds)) {
    if (onActions)
      actionCursor_ = std::max(actionCursor_ - 1, 0);
    else
      backupCursor_ = std::max(backupCursor_ - 1, 0);
    return true;
  }
  if (!keys::isConfirm(event))
    return false;
  if (onActions)
    runSelectedAction(ctx);
  return true;
}

std::vector<hint> syncTab::hints() const {
  return {{"h/l", "panel"}, {"j/k", "move"}, {"ret", "run"}, {"q", "quit"}};
}

}  // namespace catalist
