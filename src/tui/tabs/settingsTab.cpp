#include "catalist/tui/tabs/settingsTab.hpp"

#include <algorithm>

#include "catalist/storage/backup.hpp"
#include "catalist/tui/actions.hpp"
#include "catalist/tui/keys.hpp"
#include "catalist/tui/theme/palettes.hpp"
#include "catalist/tui/theme/themeRegistry.hpp"
#include "catalist/tui/widgets/card.hpp"
#include "catalist/tui/widgets/pill.hpp"

namespace catalist {

using namespace ftxui;

namespace {

constexpr int kPreferenceRows = 5;
constexpr int kLastPanel = 3;

// Accounts card rows, in order.
enum class accountAction { kMal, kAnilist, kSync };
constexpr int kAccountRows = 3;

// Data card rows, in order, before the backup list.
enum class dataAction { kImportXml, kExportXml, kExportCsv, kBackup };
constexpr int kDataActions = 4;

std::string nextInCycle(const std::vector<std::string>& cycle,
                        const std::string& current) {
  auto found = std::find(cycle.begin(), cycle.end(), current);
  if (found == cycle.end() || std::next(found) == cycle.end())
    return cycle.front();
  return *std::next(found);
}

Element valueText(const std::string& value) {
  return text(value) | bold | color(themeRegistry::active().lavender);
}

Element connectionPill(bool connected, const themePalette& theme) {
  return connected ? pill(icon::kSync + " Connected", theme.crust, theme.green)
                   : pill(icon::kOffline + " Not connected", theme.crust,
                          theme.surface2);
}

std::string maskedToken(const std::string& token) {
  if (token.empty())
    return "(paste here)";
  return std::string(std::min<size_t>(token.size(), 24), '*') + "  " +
         std::to_string(token.size()) + " characters";
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
  auto applied = std::find(themes.begin(), themes.end(), ctx.settings.theme);
  themeCursor_ = applied == themes.end()
                     ? 0
                     : static_cast<int>(applied - themes.begin());
}

void settingsTab::onDeactivate(appContext& ctx) {
  revertPreview(ctx);
  pendingRestore_.reset();
  pastedToken_.reset();
}

void settingsTab::previewThemeUnderCursor() {
  auto themes = themeRegistry::getAvailableThemes();
  if (themeCursor_ >= 0 && themeCursor_ < static_cast<int>(themes.size()))
    themeRegistry::setActive(themes[themeCursor_]);
}

void settingsTab::revertPreview(appContext& ctx) {
  themeRegistry::setActive(ctx.settings.theme);
}

void settingsTab::applySelectedTheme(appContext& ctx) {
  auto themes = themeRegistry::getAvailableThemes();
  if (themeCursor_ < 0 || themeCursor_ >= static_cast<int>(themes.size()))
    return;
  ctx.settings.theme = themes[themeCursor_];
  themeRegistry::setActive(ctx.settings.theme);
  persistSettings(ctx);
  ctx.notices.setStatus("Applied theme: " + ctx.settings.theme);
}

void settingsTab::cyclePreference(appContext& ctx) {
  auto& settings = ctx.settings;
  switch (preferenceCursor_) {
    case 0:
      settings.mouse = !settings.mouse;
      ctx.notices.setStatus(settings.mouse ? "Mouse enabled." : "Mouse disabled.");
      break;
    case 1:
      settings.startupTab =
          nextInCycle(tabIds_, settings.startupTab.value_or(settings.defaultMedia));
      ctx.notices.setStatus("Startup tab: " + *settings.startupTab);
      break;
    case 2:
      settings.defaultMedia = nextInCycle(mediaChoices(), settings.defaultMedia);
      ctx.notices.setStatus("Default media: " + settings.defaultMedia);
      break;
    case 3:
      settings.defaultSort = nextInCycle(sortChoices(), settings.defaultSort);
      ctx.notices.setStatus("Default sort: " + settings.defaultSort);
      break;
    default:
      settings.reverseSort = !settings.reverseSort;
      ctx.notices.setStatus(settings.reverseSort ? "Sort: Descending"
                                                 : "Sort: Ascending");
      break;
  }
  persistSettings(ctx);
}

void settingsTab::toggleMalAccount(appContext& ctx) {
  if (!ctx.accounts.malLogin.isAuthenticated()) {
    startMalLogin(ctx);
    return;
  }
  ctx.accounts.malLogin.logout();
  ctx.notices.setStatus("Logged out of MyAnimeList.");
}

void settingsTab::toggleAnilistAccount(appContext& ctx) {
  if (ctx.accounts.anilistLogin.isAuthenticated()) {
    ctx.accounts.anilistLogin.logout();
    ctx.notices.setStatus("Logged out of AniList.");
    return;
  }
  if (openAnilistLogin(ctx))
    pastedToken_ = std::string();
}

void settingsTab::runAccountAction(appContext& ctx) {
  switch (static_cast<accountAction>(accountCursor_)) {
    case accountAction::kMal: toggleMalAccount(ctx); break;
    case accountAction::kAnilist: toggleAnilistAccount(ctx); break;
    case accountAction::kSync: startBackgroundSync(ctx); break;
  }
}

void settingsTab::runDataAction(appContext& ctx) {
  if (dataCursor_ >= kDataActions) {
    auto snapshots =
        backup::listSnapshots(ctx.fileSystem, ctx.db.getConfigPath());
    pendingRestore_ = snapshots.at(static_cast<size_t>(dataCursor_ - kDataActions));
    ctx.notices.setStatus("Restore " + *pendingRestore_ +
                          "? [ret] confirm  [Esc] cancel");
    return;
  }
  switch (static_cast<dataAction>(dataCursor_)) {
    case dataAction::kImportXml: importXmlInteractively(ctx); break;
    case dataAction::kExportXml: exportMalXml(ctx); break;
    case dataAction::kExportCsv: exportCatalog(ctx); break;
    case dataAction::kBackup: createBackup(ctx); break;
  }
}

bool settingsTab::handleTokenPaste(const Event& event, appContext& ctx) {
  if (event == Event::Escape) {
    pastedToken_.reset();
    ctx.notices.setStatus("AniList login canceled.");
  } else if (keys::isConfirm(event)) {
    finishAnilistLogin(ctx, *pastedToken_);
    pastedToken_.reset();
  } else if (event == Event::Backspace && !pastedToken_->empty()) {
    pastedToken_->pop_back();
  } else if (event.is_character()) {
    *pastedToken_ += event.character();
  }
  return true;
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
  switch (activePanel_) {
    case panel::kThemes:
      return static_cast<int>(themeRegistry::getAvailableThemes().size()) - 1;
    case panel::kPreferences:
      return kPreferenceRows - 1;
    case panel::kAccounts:
      return kAccountRows - 1;
    case panel::kData:
      break;
  }
  auto snapshots = backup::listSnapshots(ctx.fileSystem, ctx.db.getConfigPath());
  return kDataActions - 1 + static_cast<int>(snapshots.size());
}

int& settingsTab::cursorOfActivePanel() {
  switch (activePanel_) {
    case panel::kThemes: return themeCursor_;
    case panel::kPreferences: return preferenceCursor_;
    case panel::kAccounts: return accountCursor_;
    case panel::kData: break;
  }
  return dataCursor_;
}

void settingsTab::moveCursor(int delta, appContext& ctx) {
  int last = std::max(0, lastRow(ctx));
  int& cursor = cursorOfActivePanel();
  cursor = std::clamp(cursor + delta, 0, last);
  if (activePanel_ == panel::kThemes)
    previewThemeUnderCursor();
}

void settingsTab::activateRow(appContext& ctx) {
  switch (activePanel_) {
    case panel::kThemes: applySelectedTheme(ctx); break;
    case panel::kPreferences: cyclePreference(ctx); break;
    case panel::kAccounts: runAccountAction(ctx); break;
    case panel::kData: runDataAction(ctx); break;
  }
}

bool settingsTab::onEvent(const Event& event, appContext& ctx) {
  if (pastedToken_)
    return handleTokenPaste(event, ctx);
  if (pendingRestore_)
    return handlePendingRestore(event, ctx);
  if (keys::isMoveLeft(event) || keys::isMoveRight(event)) {
    int step = keys::isMoveLeft(event) ? -1 : 1;
    revertPreview(ctx);
    int panelIndex = static_cast<int>(activePanel_) + step;
    activePanel_ = static_cast<panel>(std::clamp(panelIndex, 0, kLastPanel));
    return true;
  }
  if (keys::isMoveDown(event) || keys::isMoveUp(event)) {
    moveCursor(keys::isMoveDown(event) ? 1 : -1, ctx);
    return true;
  }
  if (keys::isJumpTop(event) || keys::isJumpBottom(event)) {
    moveCursor(keys::isJumpTop(event) ? -1000 : 1000, ctx);
    return true;
  }
  if (!keys::isConfirmOrSpace(event))
    return false;
  activateRow(ctx);
  return true;
}

Element settingsTab::renderThemes(appContext& ctx) const {
  const auto& theme = themeRegistry::active();
  bool focused = activePanel_ == panel::kThemes;
  Elements rows = {card::section("COLOUR SCHEMES")};
  auto themes = themeRegistry::getAvailableThemes();
  for (size_t i = 0; i < themes.size(); ++i) {
    bool applied = ctx.settings.theme == themes[i];
    bool cursorHere = focused && themeCursor_ == static_cast<int>(i);
    std::string mark = applied ? icon::kCheckSingle : icon::kCheckBlank;
    rows.push_back(card::choice(cursorHere, mark + " " + themes[i],
                                applied ? theme.mauve : theme.text));
  }
  auto body = vbox({vbox(std::move(rows)) | vscroll_indicator | frame | flex,
                    card::note("[ret] keeps the preview")});
  return card::frame("Themes", body, focused) | size(WIDTH, EQUAL, 28);
}

Element settingsTab::renderPreferences(appContext& ctx) const {
  const auto& settings = ctx.settings;
  bool focused = activePanel_ == panel::kPreferences;
  auto row = [&](int index, const std::string& name, const std::string& value) {
    return card::field(focused && preferenceCursor_ == index, name,
                       valueText(value), 16);
  };
  auto body = vbox(
      {card::section("BEHAVIOUR"),
       row(0, "Mouse:", settings.mouse ? "Enabled" : "Disabled"),
       row(1, "Startup tab:", settings.startupTab.value_or(settings.defaultMedia)),
       text(""), card::section("CATALOGUE"),
       row(2, "Default media:", settings.defaultMedia),
       row(3, "Default sort:", settings.defaultSort),
       row(4, "Sort order:", settings.reverseSort ? "Descending ▼" : "Ascending ▲"),
       text(""), card::note("[ret] cycles the value; saved to settings.json")});
  return card::frame("PREFERENCES", body, focused);
}

Element settingsTab::renderTokenPrompt() const {
  const auto& theme = themeRegistry::active();
  return vbox({card::note("Approve Catalist in the browser, then paste the"),
               card::note("token AniList shows and press [ret]."),
               hbox({text("  "),
                     text(" " + maskedToken(*pastedToken_) + " ") |
                         color(theme.text) | border | color(theme.peach) | flex,
                     text("  ")})});
}

Element settingsTab::renderAccounts(appContext& ctx) const {
  const auto& theme = themeRegistry::active();
  bool focused = activePanel_ == panel::kAccounts;
  auto active = [&](accountAction action) {
    return focused && accountCursor_ == static_cast<int>(action);
  };
  bool malIn = ctx.accounts.malLogin.isAuthenticated();
  bool anilistIn = ctx.accounts.anilistLogin.isAuthenticated();
  bool syncing = isSyncing(ctx);
  std::string anilistHint =
      ctx.accounts.anilistLogin.hasClientId()
          ? (anilistIn ? "[ret] log out" : "[ret] log in")
          : "needs CATALIST_ANILIST_CLIENT_ID";
  Elements rows = {
      card::section("MYANIMELIST"),
      card::field(active(accountAction::kMal), "Account:",
                  hbox({connectionPill(malIn, theme), filler(),
                        text(malIn ? "[ret] log out  " : "[ret] log in  ") |
                            color(theme.overlay0)}),
                  16),
      text(""), card::section("ANILIST"),
      card::field(active(accountAction::kAnilist), "Account:",
                  hbox({connectionPill(anilistIn, theme), filler(),
                        text(anilistHint + "  ") | color(theme.overlay0)}),
                  16)};
  if (pastedToken_)
    rows.push_back(renderTokenPrompt());
  rows.push_back(text(""));
  rows.push_back(card::section("TWO-WAY SYNC"));
  rows.push_back(card::field(
      active(accountAction::kSync), "Sync now:",
      text(syncing ? "Syncing..." : "[ret] pull, push edits and deletions") |
          color(syncing ? theme.yellow : theme.text),
      16));
  rows.push_back(filler());
  rows.push_back(card::note("A backup is taken before every sync."));
  return card::frame("ACCOUNTS & SYNC", vbox(std::move(rows)), focused,
                     syncing ? "[Syncing]" : "");
}

Element settingsTab::renderData(appContext& ctx) const {
  const auto& theme = themeRegistry::active();
  bool focused = activePanel_ == panel::kData;
  auto row = [&](int index, const std::string& label) {
    return card::choice(focused && dataCursor_ == index, label, theme.text);
  };
  Elements rows = {card::section("IMPORT"),
                   row(0, "󰈞 MyAnimeList XML file"),
                   text(""), card::section("EXPORT"),
                   row(1, "󰈝 MyAnimeList XML (MAL / AniList)"),
                   row(2, "󰈝 Catalogue as CSV"),
                   text(""), card::section("BACKUPS"),
                   row(3, "󰆓 Create a snapshot now")};
  auto snapshots = backup::listSnapshots(ctx.fileSystem, ctx.db.getConfigPath());
  for (size_t i = 0; i < snapshots.size(); ++i)
    rows.push_back(row(kDataActions + static_cast<int>(i), "  " + snapshots[i]));
  if (snapshots.empty())
    rows.push_back(card::note("  none yet"));

  std::string running = ctx.jobs.runningName();
  auto body = vbox({vbox(std::move(rows)) | vscroll_indicator | frame | flex,
                    card::note(running.empty() ? "[ret] runs; on a snapshot, restores."
                                               : "Running: " + running + "...")});
  return card::frame("DATA", body, focused) | size(WIDTH, EQUAL, 42);
}

Element settingsTab::render(appContext& ctx) {
  return hbox({renderThemes(ctx),
               vbox({renderPreferences(ctx), renderAccounts(ctx) | flex}) | flex,
               renderData(ctx)});
}

std::vector<hint> settingsTab::hints() const {
  if (pastedToken_)
    return {{"ret", "save token"}, {"Esc", "cancel"}};
  if (pendingRestore_)
    return {{"ret", "restore"}, {"Esc", "cancel"}};
  return {{"h/l", "card"}, {"j/k", "move"}, {"ret", "apply"}, {"q", "quit"}};
}

}  // namespace catalist
