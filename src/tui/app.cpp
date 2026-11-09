#include "catalist/tui/app.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
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
#include "catalist/tui/tabRegistry.hpp"
#include "catalist/tui/tabs/catalogTab.hpp"
#include "catalist/tui/tabs/helpTab.hpp"
#include "catalist/tui/tabs/historyTab.hpp"
#include "catalist/tui/tabs/libraryTab.hpp"
#include "catalist/tui/tabs/queueTab.hpp"
#include "catalist/tui/tabs/searchTab.hpp"
#include "catalist/tui/tabs/settingsTab.hpp"
#include "catalist/tui/tabs/statsTab.hpp"
#include "catalist/tui/theme/palettes.hpp"
#include "catalist/tui/theme/themeRegistry.hpp"
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
    if (event == Event::Custom)
      return true;
    if (event.is_mouse() && !ctx_.config.mouse)
      return true;
    if (ctx_.notices.hasPopup()) {
      if (isPopupDismissal(event))
        ctx_.notices.dismiss();
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
    auto base = vbox({renderTopBar(theme), activeTab().render(ctx_) | flex,
                      renderBottomBar(theme)});
    if (auto popup = ctx_.notices.renderPopup(theme))
      return dbox({base, *popup});
    if (palette_.isOpen())
      return dbox({base, palette_.render(theme)});
    if (auto modal = activeTab().overlay(ctx_))
      return dbox({base, *modal});
    return base;
  }

private:
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
    if (keys::isBound(event, ctx_.config.keybinds, keyAction::kCommandPalette)) {
      palette_.open();
      return true;
    }
    if (keys::isBound(event, ctx_.config.keybinds, keyAction::kOpenHelp)) {
      openTab("help");
      return true;
    }
    bool quits = keys::isCharacter(event, 'q') ||
                 keys::isBound(event, ctx_.config.keybinds, keyAction::kQuit);
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
      ctx_.notices.setStatus("Current theme: " + ctx_.config.theme);
      return;
    }
    if (!themeRegistry::hasTheme(themeName)) {
      ctx_.notices.setStatus("Unknown theme: " + themeName);
      return;
    }
    ctx_.config.theme = themeName;
    themeRegistry::setActive(themeName);
    saveConfig(ctx_);
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
    } else if (verb == "help") {
      openTab("help");
    } else if (verb == "tab") {
      openTab(argument);
    } else {
      ctx_.notices.setStatus("Command executed: " + verb);
    }
    activeTab().onActivate(ctx_);
  }

  Element renderTopBar(const themePalette& theme) {
    Elements items;
    auto addTab = [&](iTab& tab, const std::string& label,
                      const std::string& glyph, bool showCount) {
      bool active = &tab == &activeTab();
      int number = indexOf(tabs_, tab.id()) + 1;
      std::string count = showCount ? tab.countBadge(ctx_).value_or("") : "";
      items.push_back(tabLabel(number, active, label, glyph, count, theme));
      items.push_back(text("│") | color(theme.surface0));
    };
    auto version = text(std::string("v") + kVersionString) | color(theme.overlay0);

    auto topBar = ctx_.config.kdlDoc.getNode("top-bar");
    bool configured = topBar && !topBar->children.empty();
    for (const auto& item : configured ? topBar->children
                                       : std::vector<kdl::node>{}) {
      if (item.name == "tab") {
        int index = indexOf(tabs_, item.getArgString(0));
        if (index < 0)
          continue;
        iTab& tab = *tabs_[index];
        addTab(tab, item.getPropString("label", tab.title()),
               item.getPropString("icon", tab.icon()),
               item.getPropBool("show-count", false));
      } else if (item.name == "spacer") {
        items.push_back(filler());
      } else if (item.name == "text") {
        items.push_back(text(item.getArgString(0)) | color(theme.overlay0));
      } else if (item.name == "version") {
        items.push_back(version);
      }
    }
    if (!configured) {
      for (iTab* tab : tabs_)
        addTab(*tab, tab->title(), tab->icon(), tab->countBadge(ctx_).has_value());
      items.push_back(filler());
      items.push_back(hbox({version, text(" ")}));
    }
    return hbox(std::move(items)) | border | color(theme.surface0) |
           size(HEIGHT, EQUAL, 3);
  }

  Element renderBottomBar(const themePalette& theme) {
    Element account =
        ctx_.auth.isAuthenticated()
            ? pill(icon::kSync + " MAL: Connected", theme.crust, theme.green)
            : pill(icon::kOffline + " Offline", Color::Black, theme.surface2);

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

    int maxStatusLength = std::max(16, Terminal::Size().dimx - 76 - 28);
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
};

void registerTabs(tabRegistry& registry, const appConfig& config) {
  registry.add(std::make_unique<catalogTab>(mediaType::kAnime, config));
  registry.add(std::make_unique<catalogTab>(mediaType::kManga, config));
  registry.add(std::make_unique<queueTab>());
  registry.add(std::make_unique<searchTab>(config));
  registry.add(std::make_unique<libraryTab>());
  registry.add(std::make_unique<historyTab>());
  registry.add(std::make_unique<statsTab>());
  registry.add(std::make_unique<settingsTab>(builtInTabIds()));
  registry.add(std::make_unique<helpTab>());
}

std::string chooseStartupTab(const appConfig& config,
                             const std::optional<std::string>& requested) {
  if (requested)
    return *requested;
  return config.startupTab.value_or(config.defaultMedia);
}

void greetFirstRun(appContext& ctx) {
  if (ctx.config.welcomeShown)
    return;
  ctx.notices.push(
      "Welcome to Catalist! Number keys switch tabs, : opens commands and "
      "? shows every key.");
  ctx.config.welcomeShown = true;
  saveConfig(ctx);
}

}  // namespace

std::vector<std::string> builtInTabIds() {
  return {"anime",   "manga", "queue",    "search", "library",
          "history", "stats", "settings", "help"};
}

appTui::appTui(database& db, appConfig& config, tuiServices services)
    : db_(db), config_(config), services_(services) {}

void appTui::run(const std::optional<std::string>& startupTab) {
  themeRegistry::setActive(config_.theme);
  auto screen = ScreenInteractive::Fullscreen();
  jobRunner jobs(notices_, [&screen] { screen.PostEvent(Event::Custom); });
  shell* activeShell = nullptr;
  appContext ctx{db_,
                 config_,
                 undo_,
                 notices_,
                 services_.launcher,
                 services_.clock,
                 services_.fileSystem,
                 services_.http,
                 services_.providers,
                 services_.auth,
                 jobs,
                 [&activeShell](const std::string& tabId) {
                   if (activeShell)
                     activeShell->openTab(tabId);
                 }};

  tabRegistry registry;
  registerTabs(registry, config_);
  auto tabs = registry.ordered(config_.kdlDoc);
  std::vector<std::string> titles;
  for (iTab* tab : tabs)
    titles.push_back(tab->title());
  if (auto* help = dynamic_cast<helpTab*>(registry.find("help")))
    help->setTabTitles(titles);

  Components tabComponents;
  for (iTab* tab : tabs)
    tabComponents.push_back(makeTabComponent(*tab, ctx));

  shell app(ctx, tabs, palette_, screen);
  activeShell = &app;
  int startIndex = indexOf(tabs, chooseStartupTab(config_, startupTab));
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
