#include "catalist/tui/app.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/terminal.hpp>
#include <sstream>
#include <thread>

#include "catalist/core/logger.hpp"
#include "catalist/core/version.hpp"
#include "catalist/tui/actions.hpp"
#include "catalist/tui/appContext.hpp"
#include "catalist/tui/keys.hpp"
#include "catalist/tui/tabs/catalogTab.hpp"
#include "catalist/tui/tabs/settingsTab.hpp"
#include "catalist/tui/tabs/statsTab.hpp"
#include "catalist/tui/theme/palettes.hpp"
#include "catalist/tui/theme/themeRegistry.hpp"
#include "catalist/tui/widgets/card.hpp"
#include "catalist/tui/widgets/pill.hpp"

namespace catalist {

using namespace ftxui;

namespace {

// Redraw cadence for the scrolling titles in the catalogue.
constexpr auto kTickInterval = std::chrono::milliseconds(250);

bool isBackTab(const Event& event) {
  return event == Event::TabReverse || event == Event::Special("\033[Z");
}

bool isPopupDismissal(const Event& event) {
  return event == Event::Character(';') || event == Event::Escape ||
         event == Event::Return;
}

int indexOf(const std::vector<iTab*>& tabs, const std::string& tabId) {
  for (size_t i = 0; i < tabs.size(); ++i) {
    if (tabs[i]->id() == tabId)
      return static_cast<int>(i);
  }
  return -1;
}

Element tabLabel(int number, bool active, const std::string& label,
                 const std::string& glyph, const std::string& count,
                 const themePalette& theme) {
  std::string shown = " " + std::to_string(number) + " " + glyph + " " + label;
  if (!count.empty())
    shown += " (" + count + ")";
  shown += " ";
  return text(shown) | (active ? bold | color(theme.mauve) : color(theme.subtext));
}

std::string truncated(std::string message, int maxLength) {
  if (static_cast<int>(message.size()) <= maxLength)
    return message;
  return message.substr(0, std::max(0, maxLength - 3)) + "...";
}

class shell {
public:
  shell(appContext& ctx, std::vector<iTab*> tabs, commandPalette& palette,
        ScreenInteractive& screen)
      : ctx_(ctx), tabs_(std::move(tabs)), palette_(palette), screen_(screen) {}

  int& activeIndex() { return activeIndex_; }
  iTab& activeTab() { return *tabs_[activeIndex_]; }

  void activate(int index) {
    if (index < 0 || index >= static_cast<int>(tabs_.size()))
      return;
    if (index != activeIndex_)
      activeTab().onDeactivate(ctx_);
    activeIndex_ = index;
    activeTab().onActivate(ctx_);
  }

  void openTab(const std::string& tabId) {
    int index = indexOf(tabs_, tabId);
    if (index >= 0)
      activate(index);
  }

  bool onEvent(const Event& event) {
    if (ctx_.db.takeSaveFailure())
      ctx_.notices.push("Could not save the catalogue to " +
                        ctx_.db.getConfigPath().string() + ".");
    if (event == Event::Custom) {
      refreshAfterSync();
      return true;
    }
    if (event.is_mouse() && !ctx_.settings.mouse)
      return true;
    if (ctx_.notices.hasPopup()) {
      if (isPopupDismissal(event))
        ctx_.notices.dismiss();
      return true;
    }
    if (isSyncing(ctx_)) {
      ctx_.notices.setStatus("Sync in progress; the catalogue is back when it ends.");
      return true;
    }
    if (palette_.isOpen())
      return palette_.onEvent(event, [this](const std::string& command) {
        runCommand(command);
      });
    if (activeTab().capturesInput())
      return false;
    return handleGlobalKey(event);
  }

  Element render() {
    const auto& theme = themeRegistry::active();
    // The sync job writes the catalogue on its own thread, so nothing that
    // reads entries is drawn until it ends.
    bool syncing = isSyncing(ctx_);
    Element body = syncing ? renderSyncCard() : activeTab().render(ctx_);
    auto base = vbox({renderTopBar(theme, !syncing), body | flex,
                      renderBottomBar(theme)});
    if (auto popup = ctx_.notices.renderPopup(theme))
      return dbox({base, *popup});
    if (palette_.isOpen())
      return dbox({base, palette_.render(theme)});
    return base;
  }

private:
  void refreshAfterSync() {
    bool syncing = isSyncing(ctx_);
    if (wasSyncing_ && !syncing)
      activeTab().onActivate(ctx_);
    wasSyncing_ = syncing;
  }

  Element renderSyncCard() {
    auto body = vbox({card::note("Pulling your MyAnimeList and AniList lists,"),
                      card::note("sending local edits and deletions."),
                      text(""),
                      card::note("A backup was taken first; the result appears"),
                      card::note("here when the sync ends.")});
    return card::frame(icon::kSync + " SYNCING", body, true) |
           size(WIDTH, EQUAL, 56) | size(HEIGHT, EQUAL, 10) | center | flex;
  }

  bool handleGlobalKey(const Event& event) {
    int count = static_cast<int>(tabs_.size());
    if (event == Event::Tab) {
      activate((activeIndex_ + 1) % count);
      return true;
    }
    if (isBackTab(event)) {
      activate((activeIndex_ + count - 1) % count);
      return true;
    }
    if (switchByHotkey(event))
      return true;
    if (keys::isBound(event, keyAction::kCommandPalette)) {
      palette_.open();
      return true;
    }
    bool quits = keys::isCharacter(event, 'q') ||
                 keys::isBound(event, keyAction::kQuit);
    if (quits)
      quit();
    return quits;
  }

  bool switchByHotkey(const Event& event) {
    if (!event.is_character() || event.character().size() != 1)
      return false;
    char typed = event.character()[0];
    int position = typed - '1';
    if (typed < '1' || typed > '9' || position >= static_cast<int>(tabs_.size()))
      return false;
    activate(position);
    return true;
  }

  void quit() {
    ctx_.db.saveAll();
    screen_.ExitLoopClosure()();
  }

  void applyTheme(const std::string& themeName) {
    if (themeName.empty()) {
      ctx_.notices.setStatus("Current theme: " + ctx_.settings.theme);
      return;
    }
    if (!themeRegistry::hasTheme(themeName)) {
      ctx_.notices.setStatus("Unknown theme: " + themeName);
      return;
    }
    ctx_.settings.theme = themeName;
    themeRegistry::setActive(themeName);
    persistSettings(ctx_);
    ctx_.notices.setStatus("Applied theme: " + themeName);
  }

  void runCommand(const std::string& commandLine) {
    LOG_INFO("Command Palette dispatch: " + commandLine);
    std::stringstream words(commandLine);
    std::string verb;
    std::string argument;
    words >> verb >> argument;

    if (verb == "theme") {
      applyTheme(argument);
    } else if (verb == "q" || verb == "quit" || verb == "wq") {
      quit();
    } else if (verb == "w" || verb == "write") {
      ctx_.db.saveAll();
      ctx_.notices.push("Saved watchlists to disk successfully.");
    } else if (verb == "backup") {
      createBackup(ctx_);
    } else if (verb == "sync") {
      startBackgroundSync(ctx_);
    } else if (verb == "import") {
      importXmlInteractively(ctx_);
    } else if (verb == "about") {
      ctx_.notices.push(aboutLine(ctx_));
    } else if (verb == "tab") {
      openTab(argument);
    } else {
      ctx_.notices.setStatus("Command executed: " + verb);
    }
    activeTab().onActivate(ctx_);
  }

  Element renderTopBar(const themePalette& theme, bool showCounts) {
    Elements items;
    for (size_t i = 0; i < tabs_.size(); ++i) {
      iTab& tab = *tabs_[i];
      bool active = &tab == &activeTab();
      std::string count = showCounts ? tab.countBadge(ctx_).value_or("") : "";
      items.push_back(tabLabel(static_cast<int>(i) + 1, active, tab.title(),
                               tab.icon(), count, theme));
      items.push_back(text("│") | color(theme.surface0));
    }
    items.push_back(filler());
    items.push_back(text(std::string("v") + kVersionString + " ") |
                    color(theme.overlay0));
    return hbox(std::move(items)) | border | color(theme.surface0) |
           size(HEIGHT, EQUAL, 3);
  }

  Element renderBottomBar(const themePalette& theme) {
    auto accountPill = [&](const std::string& name, bool connected) {
      return connected ? pill(icon::kSync + " " + name, theme.crust, theme.green)
                       : pill(icon::kOffline + " " + name, Color::Black,
                              theme.surface2);
    };
    Element account =
        hbox({accountPill("MAL", ctx_.accounts.malLogin.isAuthenticated()),
              text(" "),
              accountPill("AniList", ctx_.accounts.anilistLogin.isAuthenticated())});

    Elements hintItems;
    for (const auto& item : activeTab().hints()) {
      if (!hintItems.empty())
        hintItems.push_back(text("  "));
      hintItems.push_back(hbox({text(item.key) | bold | color(theme.text),
                                text(" " + item.description) |
                                    color(theme.subtext)}));
    }

    std::string selection = activeTab().selectionSummary();
    Element selectionElement =
        selection.empty() ? text("")
                          : hbox({text(" " + selection + " ") | bold |
                                      color(theme.green),
                                  text("│ ") | color(theme.surface1)});

    int maxStatusLength = std::max(16, Terminal::Size().dimx - 76 - 40);
    return hbox({text(" "), account, text(" │ ") | color(theme.surface1),
                 selectionElement,
                 text(truncated(ctx_.notices.status(), maxStatusLength)) |
                     color(theme.yellow),
                 filler(), hbox(std::move(hintItems)), text(" ")}) |
           border | color(theme.surface0) | size(HEIGHT, EQUAL, 3);
  }

  appContext& ctx_;
  std::vector<iTab*> tabs_;
  commandPalette& palette_;
  ScreenInteractive& screen_;
  int activeIndex_ = 0;
  bool wasSyncing_ = false;
};

std::vector<std::unique_ptr<iTab>> makeTabs(const appSettings& settings) {
  std::vector<std::unique_ptr<iTab>> made;
  made.push_back(std::make_unique<catalogTab>(mediaType::kAnime, settings));
  made.push_back(std::make_unique<catalogTab>(mediaType::kManga, settings));
  made.push_back(std::make_unique<statsTab>());
  made.push_back(std::make_unique<settingsTab>(builtInTabIds()));
  return made;
}

std::string chooseStartupTab(const appSettings& settings,
                             const std::optional<std::string>& requested) {
  if (requested)
    return *requested;
  return settings.startupTab.value_or(settings.defaultMedia);
}

void greetFirstRun(appContext& ctx) {
  if (ctx.settings.welcomeShown)
    return;
  ctx.notices.push(
      "Welcome to Catalist! Number keys switch tabs and : opens commands.");
  ctx.settings.welcomeShown = true;
  persistSettings(ctx);
}

}  // namespace

std::vector<std::string> builtInTabIds() {
  return tabChoices();
}

appTui::appTui(database& db, appSettings& settings, tuiServices services)
    : db_(db), settings_(settings), services_(services) {}

void appTui::run(const std::optional<std::string>& startupTab) {
  themeRegistry::setActive(settings_.theme);
  auto screen = ScreenInteractive::Fullscreen();
  jobRunner jobs(notices_, [&screen] { screen.PostEvent(Event::Custom); });
  appContext ctx{db_,
                 settings_,
                 undo_,
                 notices_,
                 services_.launcher,
                 services_.clock,
                 services_.fileSystem,
                 services_.http,
                 services_.accounts,
                 jobs};

  auto ownedTabs = makeTabs(settings_);
  std::vector<iTab*> tabs;
  for (auto& tab : ownedTabs)
    tabs.push_back(tab.get());
  Components tabComponents;
  for (iTab* tab : tabs)
    tabComponents.push_back(makeTabComponent(*tab, ctx));

  shell app(ctx, tabs, palette_, screen);
  int startIndex = indexOf(tabs, chooseStartupTab(settings_, startupTab));
  if (startIndex < 0)
    notices_.setStatus("Unknown startup tab; showing the first tab.");
  app.activate(std::max(0, startIndex));
  greetFirstRun(ctx);

  auto tabContainer = Container::Tab(tabComponents, &app.activeIndex());
  auto withGlobalKeys = CatchEvent(tabContainer, [&app](Event event) {
    return app.onEvent(event);
  });
  auto root = Renderer(withGlobalKeys, [&app] { return app.render(); });

  std::atomic<bool> running{true};
  std::thread ticker([&]() {
    while (running.load()) {
      std::this_thread::sleep_for(kTickInterval);
      screen.PostEvent(Event::Custom);
    }
  });
  screen.Loop(root);
  running = false;
  ticker.join();
}

}  // namespace catalist
