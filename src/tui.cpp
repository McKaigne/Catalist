#include "catalist/tui.hpp"
#include "catalist/malParser.hpp"
#include "catalist/sync.hpp"
#include "catalist/remoteSearch.hpp"

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/box.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/component/event.hpp>

#include <algorithm>
#include <cmath>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>
#include <atomic>
#include <iostream>
#include <set>
#include <filesystem>

namespace catalist {

using namespace ftxui;

static std::string clipTitle(const std::string& str, size_t maxCodePoints, size_t offset = 0) {
  std::vector<std::string> glyphs;
  size_t byteIdx = 0;
  while (byteIdx < str.size()) {
    unsigned char c = static_cast<unsigned char>(str[byteIdx]);
    size_t step = 1;
    if ((c & 0x80) == 0) step = 1;
    else if ((c & 0xE0) == 0xC0) step = 2;
    else if ((c & 0xF0) == 0xE0) step = 3;
    else if ((c & 0xF8) == 0xF0) step = 4;

    if (byteIdx + step > str.size()) step = str.size() - byteIdx;
    glyphs.push_back(str.substr(byteIdx, step));
    byteIdx += step;
  }

  if (glyphs.size() <= maxCodePoints) return str;

  std::string res = "";
  size_t start = offset % glyphs.size();
  for (size_t i = 0; i < maxCodePoints; ++i) {
    res += glyphs[(start + i) % glyphs.size()];
  }
  return res;
}

static std::string formatCommas(long long n) {
  std::string s = std::to_string(n);
  int insertPosition = static_cast<int>(s.length()) - 3;
  while (insertPosition > 0) {
    s.insert(insertPosition, ",");
    insertPosition -= 3;
  }
  return s;
}

static std::string formatDays(double days) {
  long long whole = static_cast<long long>(days);
  int frac = static_cast<int>(std::round((days - whole) * 10.0)) % 10;
  return formatCommas(whole) + "." + std::to_string(frac);
}

static std::string formatSimpleDate(const std::string& dateStr) {
  if (dateStr.empty() || dateStr.find("0000") == 0) return "-       ";
  if (dateStr.size() >= 7) return dateStr.substr(0, 7) + " ";
  std::string padded = dateStr;
  while (padded.size() < 8) padded += " ";
  return padded;
}

static std::string getFormattedTimestamp() {
  auto now = std::chrono::system_clock::now();
  auto inTimeT = std::chrono::system_clock::to_time_t(now);
  auto tm = *std::localtime(&inTimeT);
  char buf[32];
  std::strftime(buf, sizeof(buf), "%b %d, %I:%M %p", &tm);
  return std::string(buf);
}

// Explicit UTF-8 byte constants for Powerline Extra rounded caps: U+E0B6 and U+E0B4
static Element makePill(const std::string& textStr, ftxui::Color fg, ftxui::Color bg, bool boldText = true) {
  static const std::string capLeft = "\xEE\x82\xB6";
  static const std::string capRight = "\xEE\x82\xB4";
  auto content = text(textStr) | color(fg) | bgcolor(bg);
  if (boldText) content = content | bold;
  return hbox({
    text(capLeft) | color(bg),
    content,
    text(capRight) | color(bg)
  });
}

static Element highlightMatches(const std::string& textStr, const std::string& query, bool isRowActive, const ThemePalette& theme) {
  if (query.empty()) {
    auto el = text(textStr) | color(theme.text);
    if (isRowActive) el = el | bold;
    return el;
  }

  std::string lowerText = textStr;
  std::string lowerQuery = query;
  std::transform(lowerText.begin(), lowerText.end(), lowerText.begin(), ::tolower);
  std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);

  size_t lastPos = 0;
  size_t pos = lowerText.find(lowerQuery, 0);

  if (pos == std::string::npos) {
    auto el = text(textStr) | color(theme.text);
    if (isRowActive) el = el | bold;
    return el;
  }

  Elements parts;
  while (pos != std::string::npos) {
    if (pos > lastPos) {
      auto nonMatch = text(textStr.substr(lastPos, pos - lastPos)) | color(theme.text);
      if (isRowActive) nonMatch = nonMatch | bold;
      parts.push_back(nonMatch);
    }
    auto matchEl = text(textStr.substr(pos, lowerQuery.size()))
                   | bold
                   | color(theme.crust)
                   | bgcolor(theme.yellow);
    parts.push_back(matchEl);
    lastPos = pos + lowerQuery.size();
    pos = lowerText.find(lowerQuery, lastPos);
  }

  if (lastPos < textStr.size()) {
    auto post = text(textStr.substr(lastPos)) | color(theme.text);
    if (isRowActive) post = post | bold;
    parts.push_back(post);
  }

  return hbox(std::move(parts));
}

static bool isSortAscending(SortKey key, bool reverse) {
  switch (key) {
    case SortKey::Score:
    case SortKey::Progress:
    case SortKey::Date:
      return reverse;
    case SortKey::Title:
    case SortKey::Status:
    case SortKey::Id:
      return !reverse;
  }
  return false;
}

static std::string getSortArrow(SortKey key, bool reverse) {
  return isSortAscending(key, reverse) ? " ▲" : " ▼";
}

AppTui::AppTui(Database& db, AppConfig& config)
    : db_(db), config_(config) {}

void AppTui::run() {
  auto screen = ScreenInteractive::Fullscreen();
  auto* screenPtr = &screen; // Decoupled pointer to eliminate non-copyable lambda capture errors

  AppMode appMode = AppMode::Nor;
  FocusedCard focusedCard = FocusedCard::Catalog;
  int detailsCursorIndex = 0; // 0: Status, 1: Rating, 2: Progress, 3: Title, 4: Notes
  ViewTab currentTab = (config_.defaultMedia == "manga") ? ViewTab::Manga : ViewTab::Anime;
  SortKey currentSort = SortKey::Score;
  bool reverseSort = config_.reverseSort;

  // Interactive Panel Cursors for Sync & Settings
  int syncActivePanel = 0;      // 0: Operations Panel, 1: Backups Repository
  int syncActionCursor = 0;     // 0: Reconcile, 1: Create Snapshot, 2: Import XML
  int syncBackupCursor = 0;

  int settingsActivePanel = 0;  // 0: Themes, 1: Preferences
  int settingsThemeCursor = 0;
  int settingsPrefCursor = 0;

  int sidebarSelected = 1;
  int catalogSelected = 0;
  std::set<WatchStatus> multiFilterStatuses;
  std::set<int> multiFilterScores;
  std::set<int> visualSelectedIds;

  std::string searchQuery = "";
  std::string inspectorTitleBuffer = "";
  std::string inspectorNotesBuffer = "";
  std::string statusMessage = "Ready.";
  int pendingDeleteId = 0;

  bool isOnlineModalOpen = false;
  std::string remoteSearchQuery = "";
  std::vector<RemoteSearchResult> remoteResults;
  std::vector<std::string> remoteResultLabels;
  int remoteSelected = 0;
  bool remoteLoading = false;
  std::string remoteStatusMsg = "";

  bool isInspectorEditingNotes = false;
  bool isInspectorEditingTitle = false;

  std::atomic<bool> syncIsRunning{false};

  std::vector<std::string> sidebarLabels;
  std::vector<std::string> catalogIndexLabels;
  std::vector<Entry> displayedEntries;

  Box tabAboutBox, tabAnimeBox, tabMangaBox, tabOverviewBox, tabSyncBox, tabSettingsBox;
  Box headerDateBox, headerTitleBox, headerStatusBox, headerRatingBox, headerProgressBox;
  Box searchBarBox;
  std::vector<Box> rowBoxes;
  std::vector<Box> sidebarBoxes;
  std::vector<Box> modalResultBoxes;

  auto copyToClipboard = [&](const std::string& str) {
    if (str.empty()) return;
    FILE* pipe = popen("wl-copy 2>/dev/null", "w");
    if (!pipe) pipe = popen("xclip -selection clipboard 2>/dev/null", "w");
    if (!pipe) pipe = popen("pbcopy 2>/dev/null", "w");
    if (pipe) {
      fwrite(str.data(), 1, str.size(), pipe);
      pclose(pipe);
    }
    statusMessage = "Copied: \"" + clipTitle(str, 24) + "\"";
  };

  auto openInBrowser = [&](const Entry& e) {
    if (e.malId <= 0) {
      statusMessage = "No online MAL ID associated with entry.";
      return;
    }
    std::string url = (e.type == MediaType::Anime)
      ? "https://myanimelist.net/anime/" + std::to_string(e.malId)
      : "https://myanimelist.net/manga/" + std::to_string(e.malId);

    std::string cmd;
#if defined(_WIN32)
    cmd = "start " + url;
#elif defined(__APPLE__)
    cmd = "open \"" + url + "\" &";
#else
    cmd = "xdg-open \"" + url + "\" >/dev/null 2>&1 &";
#endif
    int ret = std::system(cmd.c_str());
    (void)ret;
    statusMessage = "Opened: " + url;
  };

  auto recordAction = [&](HistoryAction::Type type, MediaType media, const Entry& before, const Entry& after) {
    undoStack_.push_back({type, media, before, after});
    redoStack_.clear();
  };

  auto performUndo = [&]() {
    if (undoStack_.empty()) {
      statusMessage = "Nothing to undo.";
      return;
    }
    auto act = undoStack_.back();
    undoStack_.pop_back();
    redoStack_.push_back(act);

    if (act.type == HistoryAction::Mutate) db_.updateEntry(act.before);
    else if (act.type == HistoryAction::Delete) db_.addEntry(act.before);
    else if (act.type == HistoryAction::Add) db_.removeEntry(act.media, act.after.id);
    db_.saveAll();
    statusMessage = "Action undone.";
  };

  auto performRedo = [&]() {
    if (redoStack_.empty()) {
      statusMessage = "Nothing to redo.";
      return;
    }
    auto act = redoStack_.back();
    redoStack_.pop_back();
    undoStack_.push_back(act);

    if (act.type == HistoryAction::Mutate) db_.updateEntry(act.after);
    else if (act.type == HistoryAction::Delete) db_.removeEntry(act.media, act.before.id);
    else if (act.type == HistoryAction::Add) db_.addEntry(act.after);
    db_.saveAll();
    statusMessage = "Action redone.";
  };

  auto refreshViews = [&]() {
    MediaType media = (currentTab == ViewTab::Manga) ? MediaType::Manga : MediaType::Anime;

    sidebarLabels = {
      "  LIBRARY",
      icon::Library    + " All Media    (" + std::to_string(db_.getEntries(media).size()) + ")",
      "  STATUS",
      icon::Watching   + (media == MediaType::Manga ? " Reading      (" : " Watching     (") + std::to_string(db_.countByStatus(media, WatchStatus::Watching)) + ")",
      icon::Completed  + " Completed    (" + std::to_string(db_.countByStatus(media, WatchStatus::Completed)) + ")",
      icon::OnHold     + " On-Hold      (" + std::to_string(db_.countByStatus(media, WatchStatus::OnHold)) + ")",
      icon::Dropped    + " Dropped      (" + std::to_string(db_.countByStatus(media, WatchStatus::Dropped)) + ")",
      icon::Plan       + " Planned      (" + std::to_string(db_.countByStatus(media, WatchStatus::PlanToWatch)) + ")",
      "  RATINGS",
      renderStarMeter(10) + " (" + std::to_string(db_.countByScore(media, 10)) + ")",
      renderStarMeter(9)  + " (" + std::to_string(db_.countByScore(media, 9)) + ")",
      renderStarMeter(8)  + " (" + std::to_string(db_.countByScore(media, 8)) + ")",
      renderStarMeter(7)  + " (" + std::to_string(db_.countByScore(media, 7)) + ")",
      renderStarMeter(6)  + " (" + std::to_string(db_.countByScore(media, 6)) + ")",
      renderStarMeter(5)  + " (" + std::to_string(db_.countByScore(media, 5)) + ")",
      renderStarMeter(0)  + " (" + std::to_string(db_.countByScore(media, 0)) + ")"
    };

    std::vector<Entry> pool;
    for (const auto& e : db_.getEntries(media)) {
      if (!multiFilterStatuses.empty() && multiFilterStatuses.find(e.status) == multiFilterStatuses.end()) continue;
      if (!multiFilterScores.empty() && multiFilterScores.find(e.score) == multiFilterScores.end()) continue;

      if (multiFilterStatuses.empty() && multiFilterScores.empty()) {
        if (sidebarSelected == 3 && e.status != WatchStatus::Watching) continue;
        if (sidebarSelected == 4 && e.status != WatchStatus::Completed) continue;
        if (sidebarSelected == 5 && e.status != WatchStatus::OnHold) continue;
        if (sidebarSelected == 6 && e.status != WatchStatus::Dropped) continue;
        if (sidebarSelected == 7 && e.status != WatchStatus::PlanToWatch) continue;
        if (sidebarSelected == 9 && e.score != 10) continue;
        if (sidebarSelected == 10 && e.score != 9) continue;
        if (sidebarSelected == 11 && e.score != 8) continue;
        if (sidebarSelected == 12 && e.score != 7) continue;
        if (sidebarSelected == 13 && e.score != 6) continue;
        if (sidebarSelected == 14 && e.score != 5) continue;
        if (sidebarSelected == 15 && e.score != 0) continue;
      }

      if (!searchQuery.empty()) {
        std::string tLower = e.title;
        std::string qLower = searchQuery;
        std::transform(tLower.begin(), tLower.end(), tLower.begin(), ::tolower);
        std::transform(qLower.begin(), qLower.end(), qLower.begin(), ::tolower);
        if (tLower.find(qLower) == std::string::npos) continue;
      }

      pool.push_back(e);
    }

    std::sort(pool.begin(), pool.end(), [currentSort, reverseSort](const Entry& a, const Entry& b) {
      bool res = false;
      switch (currentSort) {
        case SortKey::Score:
          if (a.score != b.score) res = a.score > b.score;
          else res = a.title < b.title;
          break;
        case SortKey::Status:
          if (a.status != b.status) res = static_cast<int>(a.status) < static_cast<int>(b.status);
          else res = a.title < b.title;
          break;
        case SortKey::Title:
          res = a.title < b.title;
          break;
        case SortKey::Progress:
          if (a.progress != b.progress) res = a.progress > b.progress;
          else res = a.title < b.title;
          break;
        case SortKey::Date:
          if (a.startDate != b.startDate) res = a.startDate > b.startDate;
          else res = a.title < b.title;
          break;
        case SortKey::Id:
          res = a.id < b.id;
          break;
      }
      return reverseSort ? !res : res;
    });

    displayedEntries = std::move(pool);
    catalogIndexLabels.clear();
    rowBoxes.assign(displayedEntries.size(), Box{});
    sidebarBoxes.assign(sidebarLabels.size(), Box{});
    for (size_t i = 0; i < displayedEntries.size(); ++i) {
      catalogIndexLabels.push_back(std::to_string(i));
    }

    if (catalogSelected >= static_cast<int>(displayedEntries.size())) {
      catalogSelected = std::max(0, static_cast<int>(displayedEntries.size()) - 1);
    }
  };

  refreshViews();

  auto openRemoteSearchModal = [&]() {
    isOnlineModalOpen = true;
    appMode = AppMode::Sch;
    remoteSearchQuery = searchQuery;
    remoteResults.clear();
    remoteResultLabels.clear();
    remoteSelected = 0;
    remoteStatusMsg = "";
    remoteLoading = false;
  };

  auto executeRemoteSearch = [&]() {
    if (remoteSearchQuery.empty()) return;
    remoteLoading = true;
    remoteStatusMsg = "Querying AniList / MyAnimeList...";
    MediaType media = (currentTab == ViewTab::Manga) ? MediaType::Manga : MediaType::Anime;
    std::string err;
    remoteResults = RemoteSearchEngine::query(media, remoteSearchQuery, err, 10);
    remoteLoading = false;

    remoteResultLabels.clear();
    modalResultBoxes.assign(remoteResults.size(), Box{});
    for (size_t i = 0; i < remoteResults.size(); ++i) {
      remoteResultLabels.push_back(std::to_string(i));
    }
    remoteSelected = 0;
    if (remoteResults.empty()) {
      remoteStatusMsg = err.empty() ? "No results found." : err;
    } else {
      remoteStatusMsg = std::to_string(remoteResults.size()) + " matches from MyAnimeList.";
    }
  };

  // Progression & Lifecycle Date Automation
  auto mutateProgress = [&](Entry& e, int dir) {
    Entry before = e;
    int oldProg = e.progress;
    int newProg = std::max(0, e.progress + dir);

    auto now = std::chrono::system_clock::now();
    auto inTimeT = std::chrono::system_clock::to_time_t(now);
    std::stringstream ssDate;
    ssDate << std::put_time(std::localtime(&inTimeT), "%Y-%m-%d");
    std::string todayStr = ssDate.str();

    if (dir > 0) {
      if (oldProg == 0 && newProg >= 1) {
        if (e.startDate.empty() || e.startDate == "0000-00-00") {
          e.startDate = todayStr;
        }
      }
      if (e.status == WatchStatus::PlanToWatch || e.status == WatchStatus::OnHold || e.status == WatchStatus::Dropped) {
        e.status = WatchStatus::Watching;
      }
      e.progress = newProg;
      if (e.totalUnits > 0 && e.progress >= e.totalUnits) {
        e.progress = e.totalUnits;
        e.status = WatchStatus::Completed;
        if (e.finishDate.empty() || e.finishDate == "0000-00-00") {
          e.finishDate = todayStr;
        }
      }
    } else if (dir < 0) {
      if (e.totalUnits > 0 && oldProg >= e.totalUnits && newProg < e.totalUnits) {
        e.status = WatchStatus::Watching;
        e.finishDate = "0000-00-00";
      }
      e.progress = newProg;
    }

    e.updatedAt = todayStr;
    recordAction(HistoryAction::Mutate, e.type, before, e);
    db_.updateEntry(e);
    db_.logActivity({getFormattedTimestamp(), e.type, e.title, e.status, e.progress, e.totalUnits, e.score});
    db_.saveAll();
    statusMessage = "Progress: " + std::to_string(e.progress) + "/" + (e.totalUnits > 0 ? std::to_string(e.totalUnits) : "?");
  };

  auto mutateStatus = [&](Entry& e, int dir) {
    Entry before = e;
    if (dir > 0) {
      switch (e.status) {
        case WatchStatus::Watching:    e.status = WatchStatus::Completed; break;
        case WatchStatus::Completed:   e.status = WatchStatus::OnHold; break;
        case WatchStatus::OnHold:      e.status = WatchStatus::Dropped; break;
        case WatchStatus::Dropped:     e.status = WatchStatus::PlanToWatch; break;
        case WatchStatus::PlanToWatch: e.status = WatchStatus::Watching; break;
      }
    } else {
      switch (e.status) {
        case WatchStatus::Watching:    e.status = WatchStatus::PlanToWatch; break;
        case WatchStatus::Completed:   e.status = WatchStatus::Watching; break;
        case WatchStatus::OnHold:      e.status = WatchStatus::Completed; break;
        case WatchStatus::Dropped:     e.status = WatchStatus::OnHold; break;
        case WatchStatus::PlanToWatch: e.status = WatchStatus::Dropped; break;
      }
    }
    recordAction(HistoryAction::Mutate, e.type, before, e);
    db_.updateEntry(e);
    db_.logActivity({getFormattedTimestamp(), e.type, e.title, e.status, e.progress, e.totalUnits, e.score});
    db_.saveAll();
    statusMessage = "Status: " + statusToString(e.status, e.type);
  };

  auto mutateRating = [&](Entry& e, int dir) {
    Entry before = e;
    e.score = std::clamp(e.score + dir, 0, 10);
    recordAction(HistoryAction::Mutate, e.type, before, e);
    db_.updateEntry(e);
    db_.logActivity({getFormattedTimestamp(), e.type, e.title, e.status, e.progress, e.totalUnits, e.score});
    db_.saveAll();
    statusMessage = "Rating: " + renderStarMeter(e.score);
  };

  auto toggleColumnSort = [&](SortKey targetKey, const std::string& name) {
    if (currentSort == targetKey) {
      reverseSort = !reverseSort;
    } else {
      currentSort = targetKey;
      if (targetKey == SortKey::Score || targetKey == SortKey::Progress || targetKey == SortKey::Date) {
        reverseSort = true;
      } else {
        reverseSort = false;
      }
    }
    refreshViews();
    bool asc = isSortAscending(currentSort, reverseSort);
    statusMessage = "Sort: " + name + (asc ? " (Increasing ▲)" : " (Decreasing ▼)");
  };

  // Background Non-Blocking Sync Pipeline
  auto triggerAsyncSync = [&]() {
    if (syncIsRunning.load()) {
      statusMessage = "Sync already active in background...";
      return;
    }
    syncIsRunning.store(true);
    statusMessage = "Reconciling ongoing titles with MyAnimeList...";

    std::thread([&, screenPtr]() {
      std::string outMsg;
      MetadataSync::syncOngoing(db_, outMsg);
      syncIsRunning.store(false);
      statusMessage = outMsg;
      screenPtr->PostEvent(Event::Custom);
    }).detach();
  };

  auto dispatchCommand = [&](const std::string& cmdLine) {
    if (cmdLine.empty()) return;
    std::stringstream ss(cmdLine);
    std::string verb;
    ss >> verb;

    if (verb == "theme") {
      std::string themeName;
      ss >> themeName;
      if (themeName.empty()) {
        statusMessage = "Current theme: " + config_.theme;
        return;
      }
      if (ThemeRegistry::hasTheme(themeName)) {
        config_.theme = themeName;
        ThemeRegistry::setActive(themeName);
        config_.save(Database::getConfigDir() / "config.toml");
        statusMessage = "Applied theme: " + themeName;
      } else {
        statusMessage = "Unknown theme: " + themeName;
      }
    } else if (verb == "q" || verb == "quit") {
      db_.saveAll();
      screen.ExitLoopClosure()();
    } else if (verb == "w" || verb == "write") {
      db_.saveAll();
      statusMessage = "Saved watchlists to disk.";
    } else if (verb == "wq") {
      db_.saveAll();
      screen.ExitLoopClosure()();
    } else if (verb == "backup") {
      db_.createBackupSnapshot();
      statusMessage = "Created snapshot backup.";
    } else if (verb == "sync") {
      triggerAsyncSync();
    } else {
      statusMessage = "Command executed: " + verb;
    }
  };

  MenuOption sidebarOpt;
  sidebarOpt.on_change = [&]() { refreshViews(); };
  sidebarOpt.entries_option.transform = [&](const EntryState& state) {
    const auto& theme = ThemeRegistry::active();
    int idx = -1;
    for (size_t i = 0; i < sidebarLabels.size(); ++i) {
      if (sidebarLabels[i] == state.label) { idx = static_cast<int>(i); break; }
    }

    if (idx == 0 || state.label == "  LIBRARY") {
      return text("  LIBRARY") | bold | color(theme.overlay0);
    }
    if (idx == 2 || state.label == "  STATUS") {
      return text("  STATUS") | bold | color(theme.overlay0);
    }
    if (idx == 8 || state.label == "  RATINGS") {
      return text("  RATINGS") | bold | color(theme.overlay0);
    }

    bool inMulti = false;
    if (idx == 3 && multiFilterStatuses.count(WatchStatus::Watching)) inMulti = true;
    else if (idx == 4 && multiFilterStatuses.count(WatchStatus::Completed)) inMulti = true;
    else if (idx == 5 && multiFilterStatuses.count(WatchStatus::OnHold)) inMulti = true;
    else if (idx == 6 && multiFilterStatuses.count(WatchStatus::Dropped)) inMulti = true;
    else if (idx == 7 && multiFilterStatuses.count(WatchStatus::PlanToWatch)) inMulti = true;
    else if (idx >= 9 && idx <= 14 && multiFilterScores.count(19 - idx)) inMulti = true;
    else if (idx == 15 && multiFilterScores.count(0)) inMulti = true;

    // Inactive rows render clean whitespace: no empty checkbox clutter
    std::string prefix = "   ";
    if (inMulti) {
      prefix = icon::CheckMulti + " ";
    } else if (multiFilterStatuses.empty() && multiFilterScores.empty() && idx == sidebarSelected) {
      prefix = icon::CheckSingle + " ";
    }

    bool focused = state.focused && (focusedCard == FocusedCard::Sidebar);

    Element el = hbox({
      text(prefix) | color(theme.green) | bold,
      text(state.label) | (inMulti || (idx == sidebarSelected) ? bold : nothing) | color(theme.text)
    });

    if (focused) el = el | bgcolor(theme.surface0) | focus;
    else if (state.active) el = el | bgcolor(theme.surface0);

    return el;
  };
  auto sidebarMenu = Menu(&sidebarLabels, &sidebarSelected, sidebarOpt);

  InputOption searchOpt;
  searchOpt.transform = [&](InputState state) {
    const auto& theme = ThemeRegistry::active();
    Element el = state.element;
    if (state.is_placeholder) el = el | color(theme.overlay0);
    else el = el | color(theme.text);
    if (state.focused) el = el | color(theme.peach);
    return el;
  };
  auto searchInput = Input(&searchQuery, "Press '/' to search...", searchOpt);
  auto remoteSearchInput = Input(&remoteSearchQuery, "Search title and press [ret]...", searchOpt);

  MenuOption catalogOpt;
  catalogOpt.entries_option.transform = [&](const EntryState& state) {
    const auto& theme = ThemeRegistry::active();
    int idx = 0;
    try { idx = std::stoi(state.label); } catch (...) { return text(""); }
    if (idx < 0 || idx >= static_cast<int>(displayedEntries.size())) return text("");
    const auto& e = displayedEntries[idx];

    bool isRowActive = (idx == catalogSelected) && (focusedCard == FocusedCard::Catalog || focusedCard == FocusedCard::Details || appMode == AppMode::Edt);
    bool isSelected = visualSelectedIds.count(e.id) > 0;

    size_t maxChars = 28;
    std::string displayTitle = clipTitle(e.title, maxChars, 0);

    // Multi-select marked checkbox 󰄸 / Single-select marked 󰄲
    std::string marker;
    ftxui::Color markerColor = theme.green;
    if (isSelected) {
      if (visualSelectedIds.size() > 1) {
        marker = icon::CheckMulti + " ";
      } else {
        marker = icon::CheckSingle + " ";
      }
    } else if (isRowActive) {
      marker = "▌ ";
      markerColor = theme.mauve;
    } else {
      marker = "  ";
    }

    auto markerEl = text(marker) | bold | color(markerColor);

    // Uncluttered Status: bare icon + text without heavy capsule background
    auto statusElement = hbox({
      text(statusToIcon(e.status) + " ") | color(statusToColor(e.status)),
      text(statusToString(e.status, e.type)) | color(statusToColor(e.status))
    });

    auto row = hbox({
      markerEl,
      text(" " + formatSimpleDate(e.startDate)) | size(WIDTH, EQUAL, 10) | color(theme.subtext),
      text(" "),
      hbox({ text(" " + typeToIcon(e.type) + " "), highlightMatches(displayTitle, searchQuery, isRowActive, theme) }) | flex,
      text(" "),
      statusElement | size(WIDTH, EQUAL, 14),
      text(" "),
      text(" " + renderStarMeter(e.score)) | size(WIDTH, EQUAL, 12) | color(theme.yellow),
      text(" "),
      text(" " + icon::Gauge + " " + std::to_string(e.progress) + "/" + (e.totalUnits > 0 ? std::to_string(e.totalUnits) : "?")) | size(WIDTH, EQUAL, 12) | color(theme.lavender),
      text(" ")
    });

    if (isRowActive) row = row | bgcolor(theme.surface0) | focus;
    else if (isSelected) row = row | bgcolor(theme.surface0);

    return row;
  };

  MenuOption remoteMenuOpt;
  remoteMenuOpt.entries_option.transform = [&](const EntryState& state) {
    const auto& theme = ThemeRegistry::active();
    int idx = 0;
    try { idx = std::stoi(state.label); } catch (...) { return text(""); }
    if (idx < 0 || idx >= static_cast<int>(remoteResults.size())) return text("");
    const auto& r = remoteResults[idx];
    bool active = (idx == remoteSelected);

    std::string displayTitle = clipTitle(r.title, 34, 0);

    auto row = hbox({
      text(active ? "▌ " : "  ") | color(theme.mauve) | bold,
      text("[" + std::to_string(idx + 1) + "] ") | size(WIDTH, EQUAL, 5) | color(theme.peach) | bold,
      text(displayTitle) | size(WIDTH, EQUAL, 34) | color(theme.text) | (active ? bold : nothing),
      text(" "),
      text(r.year.empty() ? " -  " : r.year) | size(WIDTH, EQUAL, 6) | color(theme.subtext),
      text(" "),
      text(r.totalUnits > 0 ? (std::to_string(r.totalUnits) + " eps") : "ongoing") | size(WIDTH, EQUAL, 9) | color(theme.subtext),
      text(" "),
      text("★ " + (r.score > 0.0 ? std::to_string(r.score).substr(0, 4) : " -  ")) | size(WIDTH, EQUAL, 8) | color(theme.yellow),
      text(" "),
      text("MAL " + std::to_string(r.malId)) | size(WIDTH, EQUAL, 10) | color(theme.overlay0)
    });

    if (active) row = row | bgcolor(theme.surface0) | focus;
    if (idx >= 0 && idx < static_cast<int>(modalResultBoxes.size())) {
      row = row | reflect(modalResultBoxes[idx]);
    }
    return row;
  };

  auto catalogMenu = Menu(&catalogIndexLabels, &catalogSelected, catalogOpt);
  auto remoteMenu = Menu(&remoteResultLabels, &remoteSelected, remoteMenuOpt);
  auto inspectorTitleInput = Input(&inspectorTitleBuffer, "Title...");
  auto inspectorNotesInput = Input(&inspectorNotesBuffer, "Write notes here...");

  auto inspectorContainer = Container::Vertical({ inspectorTitleInput, inspectorNotesInput });
  auto modalContainer = Container::Vertical({ remoteSearchInput, remoteMenu });

  auto mainContainer = Container::Horizontal({
    sidebarMenu,
    catalogMenu,
    inspectorContainer,
    modalContainer
  });

  auto globalHandler = CatchEvent(mainContainer, [&](Event event) {
    if (event == Event::Custom) return true;
    if (!config_.mouse && event.is_mouse()) return true;

    // TYPING ISOLATION GUARDS
    if (commandPalette_.isOpen()) {
      if (event == Event::Escape) { commandPalette_.close(); appMode = AppMode::Nor; return true; }
      if (event == Event::Return) {
        commandPalette_.execute([&](const std::string& cmd) { dispatchCommand(cmd); appMode = AppMode::Nor; });
        return true;
      }
      if (event == Event::Tab) { commandPalette_.handleEvent("", false, false, true, false, false); return true; }
      if (event == Event::Backspace) { commandPalette_.handleEvent("", true, false, false, false, false); return true; }
      if (event == Event::ArrowUp) { commandPalette_.handleEvent("", false, false, false, true, false); return true; }
      if (event == Event::ArrowDown) { commandPalette_.handleEvent("", false, false, false, false, true); return true; }
      if (event.is_character()) { commandPalette_.handleEvent(event.character(), false, false, false, false, false); return true; }
      return true;
    }

    if (isOnlineModalOpen) {
      if (event == Event::Escape) { isOnlineModalOpen = false; appMode = AppMode::Nor; catalogMenu->TakeFocus(); return true; }
      if (event == Event::Tab) {
        if (remoteSearchInput->Focused()) remoteMenu->TakeFocus();
        else remoteSearchInput->TakeFocus();
        return true;
      }
      if (remoteSearchInput->Focused()) {
        if (event == Event::Return) { executeRemoteSearch(); remoteMenu->TakeFocus(); return true; }
        if (event == Event::ArrowDown) { remoteMenu->TakeFocus(); return true; }
        remoteSearchInput->OnEvent(event);
        return true;
      }
      if (remoteMenu->Focused()) {
        if (event == Event::Return && !remoteResults.empty() && remoteSelected >= 0 && remoteSelected < static_cast<int>(remoteResults.size())) {
          const auto& sel = remoteResults[remoteSelected];
          Entry e;
          e.id = sel.malId;
          e.malId = sel.malId;
          e.type = sel.type;
          e.title = sel.title;
          e.status = WatchStatus::Watching;
          e.score = 0;
          e.progress = 0;
          e.totalUnits = sel.totalUnits;
          e.startDate = sel.year.empty() ? "0000-00-00" : (sel.year + "-01-01");
          db_.addEntry(e);
          recordAction(HistoryAction::Add, e.type, Entry{}, e);
          db_.saveAll();
          refreshViews();

          for (size_t i = 0; i < displayedEntries.size(); ++i) {
            if (displayedEntries[i].id == e.id) { catalogSelected = static_cast<int>(i); break; }
          }
          isOnlineModalOpen = false;
          appMode = AppMode::Nor;
          catalogMenu->TakeFocus();
          statusMessage = "Added '" + e.title + "' [MAL ID " + std::to_string(e.malId) + "]!";
          return true;
        }
        if (event == Event::ArrowUp || event == Event::Character('k')) {
          if (remoteSelected > 0) remoteSelected--;
          else remoteSearchInput->TakeFocus();
          return true;
        }
        if (event == Event::ArrowDown || event == Event::Character('j')) {
          if (!remoteResults.empty() && remoteSelected < static_cast<int>(remoteResults.size()) - 1) remoteSelected++;
          return true;
        }
      }
      return true;
    }

    if (appMode == AppMode::Sch) {
      if (event == Event::Escape) {
        searchQuery = "";
        refreshViews();
        catalogMenu->TakeFocus();
        appMode = AppMode::Nor;
        statusMessage = "Search cleared.";
        return true;
      }
      if (event == Event::Return || event == Event::ArrowDown) {
        refreshViews();
        catalogMenu->TakeFocus();
        appMode = AppMode::Nor;
        statusMessage = searchQuery.empty() ? "Filter cleared." : ("Filtered: '" + searchQuery + "' (" + std::to_string(displayedEntries.size()) + " matches)");
        return true;
      }
      searchInput->OnEvent(event);
      refreshViews();
      return true;
    }

    if (isInspectorEditingNotes || isInspectorEditingTitle) {
      if (event == Event::Escape || event == Event::Return) {
        if (!displayedEntries.empty() && catalogSelected >= 0 && catalogSelected < static_cast<int>(displayedEntries.size())) {
          auto& e = displayedEntries[catalogSelected];
          Entry before = e;
          if (isInspectorEditingTitle && !inspectorTitleBuffer.empty()) e.title = inspectorTitleBuffer;
          if (isInspectorEditingNotes) e.notes = inspectorNotesBuffer;
          recordAction(HistoryAction::Mutate, e.type, before, e);
          db_.updateEntry(e);
          db_.saveAll();
          displayedEntries[catalogSelected] = e;
        }
        isInspectorEditingNotes = false;
        isInspectorEditingTitle = false;
        appMode = AppMode::Nor;
        statusMessage = "Saved text changes.";
        return true;
      }
      if (isInspectorEditingTitle) inspectorTitleInput->OnEvent(event);
      else inspectorNotesInput->OnEvent(event);
      return true;
    }

    // TOP-LEVEL TAB TRAVERSAL
    if (event == Event::Tab) {
      int next = (static_cast<int>(currentTab) + 1) % 6;
      currentTab = static_cast<ViewTab>(next);
      refreshViews();
      return true;
    }
    if (event == Event::TabReverse || event == Event::Special("\033[Z") || event == Event::Special("\x1b[Z")) {
      int prev = (static_cast<int>(currentTab) + 5) % 6;
      currentTab = static_cast<ViewTab>(prev);
      refreshViews();
      return true;
    }

    // Numeric switches
    if (event == Event::Character('0')) { currentTab = ViewTab::About; return true; }
    if (event == Event::Character('1')) { currentTab = ViewTab::Anime; catalogSelected = 0; refreshViews(); return true; }
    if (event == Event::Character('2')) { currentTab = ViewTab::Manga; catalogSelected = 0; refreshViews(); return true; }
    if (event == Event::Character('3')) { currentTab = ViewTab::Overview; return true; }
    if (event == Event::Character('4')) { currentTab = ViewTab::Sync; return true; }
    if (event == Event::Character('5')) { currentTab = ViewTab::Settings; return true; }

    // GLOBAL COMMAND PALETTE TRIGGER
    if (event == Event::Character(':')) {
      appMode = AppMode::Cmd;
      commandPalette_.open();
      return true;
    }

    // INTERACTIVE CONTROLS FOR SYNC TAB (2 PANELS)
    if (currentTab == ViewTab::Sync) {
      if (event == Event::Character('h') || event == Event::ArrowLeft) {
        syncActivePanel = 0;
        return true;
      }
      if (event == Event::Character('l') || event == Event::ArrowRight) {
        syncActivePanel = 1;
        return true;
      }
      if (event == Event::Character('j') || event == Event::ArrowDown) {
        if (syncActivePanel == 0) {
          if (syncActionCursor < 2) syncActionCursor++;
        } else {
          syncBackupCursor++;
        }
        return true;
      }
      if (event == Event::Character('k') || event == Event::ArrowUp) {
        if (syncActivePanel == 0) {
          if (syncActionCursor > 0) syncActionCursor--;
        } else {
          if (syncBackupCursor > 0) syncBackupCursor--;
        }
        return true;
      }
      if (event == Event::Return) {
        if (syncActivePanel == 0) {
          if (syncActionCursor == 0) {
            triggerAsyncSync();
          } else if (syncActionCursor == 1) {
            db_.createBackupSnapshot();
            statusMessage = "Created snapshot backup.";
          } else if (syncActionCursor == 2) {
            commandPalette_.open("import ");
          }
        }
        return true;
      }
      if (event == Event::Character('q')) {
        db_.saveAll();
        screen.ExitLoopClosure()();
        return true;
      }
      return false;
    }

    // INTERACTIVE CONTROLS FOR SETTINGS TAB (2 PANELS)
    if (currentTab == ViewTab::Settings) {
      auto themes = ThemeRegistry::getAvailableThemes();
      if (event == Event::Character('h') || event == Event::ArrowLeft) {
        settingsActivePanel = 0;
        return true;
      }
      if (event == Event::Character('l') || event == Event::ArrowRight) {
        settingsActivePanel = 1;
        return true;
      }
      if (event == Event::Character('j') || event == Event::ArrowDown) {
        if (settingsActivePanel == 0) {
          if (settingsThemeCursor < static_cast<int>(themes.size()) - 1) settingsThemeCursor++;
        } else {
          if (settingsPrefCursor < 3) settingsPrefCursor++;
        }
        return true;
      }
      if (event == Event::Character('k') || event == Event::ArrowUp) {
        if (settingsActivePanel == 0) {
          if (settingsThemeCursor > 0) settingsThemeCursor--;
        } else {
          if (settingsPrefCursor > 0) settingsPrefCursor--;
        }
        return true;
      }
      if (event == Event::Return || event == Event::Character(' ')) {
        if (settingsActivePanel == 0) {
          std::string th = themes[settingsThemeCursor];
          config_.theme = th;
          ThemeRegistry::setActive(th);
          config_.save(Database::getConfigDir() / "config.toml");
          statusMessage = "Applied theme: " + th;
        } else {
          if (settingsPrefCursor == 0) {
            config_.mouse = !config_.mouse;
            statusMessage = config_.mouse ? "Mouse enabled." : "Mouse disabled.";
          } else if (settingsPrefCursor == 1) {
            if (config_.defaultMedia == "anime") config_.defaultMedia = "manga";
            else if (config_.defaultMedia == "manga") config_.defaultMedia = "overview";
            else config_.defaultMedia = "anime";
            statusMessage = "Default view: " + config_.defaultMedia;
          } else if (settingsPrefCursor == 2) {
            if (config_.defaultSort == "score") config_.defaultSort = "title";
            else if (config_.defaultSort == "title") config_.defaultSort = "date";
            else if (config_.defaultSort == "date") config_.defaultSort = "progress";
            else config_.defaultSort = "score";
            statusMessage = "Default sort: " + config_.defaultSort;
          } else if (settingsPrefCursor == 3) {
            config_.reverseSort = !config_.reverseSort;
            statusMessage = config_.reverseSort ? "Sort: Descending (▲)" : "Sort: Ascending (▼)";
          }
          config_.save(Database::getConfigDir() / "config.toml");
        }
        return true;
      }
      if (event == Event::Character('q')) {
        db_.saveAll();
        screen.ExitLoopClosure()();
        return true;
      }
      return false;
    }

    // ABOUT & OVERVIEW TABS: Scoped strictly, no background list navigation
    if (currentTab == ViewTab::About || currentTab == ViewTab::Overview) {
      if (event == Event::Character('q')) {
        db_.saveAll();
        screen.ExitLoopClosure()();
        return true;
      }
      return false;
    }

    // =========================================================================
    // CATALOG & DETAILS INTERACTION (Anime / Manga Tabs Only)
    // =========================================================================

    // FAST FIELD CYCLING (SHIFT + S, SHIFT + R, SHIFT + P)
    if (event == Event::Character('S')) {
      if (!visualSelectedIds.empty()) {
        for (auto id : visualSelectedIds) {
          auto opt = db_.getById((currentTab == ViewTab::Manga ? MediaType::Manga : MediaType::Anime), id);
          if (opt) { auto e = opt.value(); mutateStatus(e, 1); }
        }
        refreshViews();
        statusMessage = "Batch cycled status for " + std::to_string(visualSelectedIds.size()) + " entries.";
      } else if (!displayedEntries.empty()) {
        auto& e = displayedEntries[catalogSelected];
        mutateStatus(e, 1);
      }
      return true;
    }

    if (event == Event::Character('R')) {
      if (!visualSelectedIds.empty()) {
        for (auto id : visualSelectedIds) {
          auto opt = db_.getById((currentTab == ViewTab::Manga ? MediaType::Manga : MediaType::Anime), id);
          if (opt) { auto e = opt.value(); mutateRating(e, 1); }
        }
        refreshViews();
        statusMessage = "Batch bumped rating for " + std::to_string(visualSelectedIds.size()) + " entries.";
      } else if (!displayedEntries.empty()) {
        auto& e = displayedEntries[catalogSelected];
        mutateRating(e, 1);
      }
      return true;
    }

    if (event == Event::Character('P') || event == Event::Character('+') || event == Event::Character('=')) {
      if (!visualSelectedIds.empty()) {
        for (auto id : visualSelectedIds) {
          auto opt = db_.getById((currentTab == ViewTab::Manga ? MediaType::Manga : MediaType::Anime), id);
          if (opt) { auto e = opt.value(); mutateProgress(e, 1); }
        }
        refreshViews();
        statusMessage = "Batch incremented progress for " + std::to_string(visualSelectedIds.size()) + " entries.";
      } else if (!displayedEntries.empty()) {
        auto& e = displayedEntries[catalogSelected];
        mutateProgress(e, 1);
      }
      return true;
    }

    if (event == Event::Character('-')) {
      if (!visualSelectedIds.empty()) {
        for (auto id : visualSelectedIds) {
          auto opt = db_.getById((currentTab == ViewTab::Manga ? MediaType::Manga : MediaType::Anime), id);
          if (opt) { auto e = opt.value(); mutateProgress(e, -1); }
        }
        refreshViews();
        statusMessage = "Batch decremented progress for " + std::to_string(visualSelectedIds.size()) + " entries.";
      } else if (!displayedEntries.empty()) {
        auto& e = displayedEntries[catalogSelected];
        mutateProgress(e, -1);
      }
      return true;
    }

    // COLUMN SORT TOGGLES (c, t, s, r, p)
    if (focusedCard != FocusedCard::Details) {
      if (event == Event::Character('c')) { toggleColumnSort(SortKey::Date, "Date"); return true; }
      if (event == Event::Character('t')) { toggleColumnSort(SortKey::Title, "Title"); return true; }
      if (event == Event::Character('s')) { toggleColumnSort(SortKey::Status, "Status"); return true; }
      if (event == Event::Character('r')) { toggleColumnSort(SortKey::Score, "Rating"); return true; }
      if (event == Event::Character('p')) { toggleColumnSort(SortKey::Progress, "Progress"); return true; }
    }

    // SPATIAL CARD NAVIGATION: h and l move across columns
    if (focusedCard == FocusedCard::Catalog) {
      if (event == Event::Character('h') || event == Event::ArrowLeft) {
        focusedCard = FocusedCard::Sidebar;
        sidebarMenu->TakeFocus();
        return true;
      }
      if (event == Event::Character('l') || event == Event::ArrowRight) {
        focusedCard = FocusedCard::Details;
        return true;
      }

      // SPACE exclusively toggles visual selection
      if (event == Event::Character(' ') || event == Event::Character('v')) {
        if (!displayedEntries.empty()) {
          int id = displayedEntries[catalogSelected].id;
          if (visualSelectedIds.count(id)) visualSelectedIds.erase(id);
          else visualSelectedIds.insert(id);
          appMode = visualSelectedIds.empty() ? AppMode::Nor : AppMode::Vis;
        }
        return true;
      }

      // Both 'e' AND 'Return' focus Details card
      if (event == Event::Return || event == Event::Character('e')) {
        focusedCard = FocusedCard::Details;
        statusMessage = "Details focused: j/k selects field, h/l adjusts, e/ret edits notes.";
        return true;
      }

      // List movement strictly within Catalog
      if (event == Event::Character('j') || event == Event::ArrowDown) {
        if (!displayedEntries.empty() && catalogSelected < static_cast<int>(displayedEntries.size()) - 1) {
          catalogSelected++;
        }
        return true;
      }
      if (event == Event::Character('k') || event == Event::ArrowUp) {
        if (catalogSelected > 0) catalogSelected--;
        return true;
      }
      if (event == Event::Character('g')) { catalogSelected = 0; return true; }
      if (event == Event::Character('G')) {
        if (!displayedEntries.empty()) catalogSelected = static_cast<int>(displayedEntries.size()) - 1;
        return true;
      }
    } else if (focusedCard == FocusedCard::Sidebar) {
      if (event == Event::Character('l') || event == Event::ArrowRight) {
        focusedCard = FocusedCard::Catalog;
        catalogMenu->TakeFocus();
        return true;
      }

      if (event == Event::Character('j') || event == Event::ArrowDown) {
        if (sidebarSelected < static_cast<int>(sidebarLabels.size()) - 1) {
          sidebarSelected++;
          if (sidebarSelected == 0 || sidebarSelected == 2 || sidebarSelected == 8) sidebarSelected++;
          refreshViews();
        }
        return true;
      }
      if (event == Event::Character('k') || event == Event::ArrowUp) {
        if (sidebarSelected > 1) {
          sidebarSelected--;
          if (sidebarSelected == 0 || sidebarSelected == 2 || sidebarSelected == 8) sidebarSelected--;
          refreshViews();
        }
        return true;
      }

      // Space toggles multi-filter criteria
      if (event == Event::Character(' ')) {
        if (sidebarSelected == 3) {
          if (multiFilterStatuses.count(WatchStatus::Watching)) multiFilterStatuses.erase(WatchStatus::Watching);
          else multiFilterStatuses.insert(WatchStatus::Watching);
        } else if (sidebarSelected == 4) {
          if (multiFilterStatuses.count(WatchStatus::Completed)) multiFilterStatuses.erase(WatchStatus::Completed);
          else multiFilterStatuses.insert(WatchStatus::Completed);
        } else if (sidebarSelected == 5) {
          if (multiFilterStatuses.count(WatchStatus::OnHold)) multiFilterStatuses.erase(WatchStatus::OnHold);
          else multiFilterStatuses.insert(WatchStatus::OnHold);
        } else if (sidebarSelected == 6) {
          if (multiFilterStatuses.count(WatchStatus::Dropped)) multiFilterStatuses.erase(WatchStatus::Dropped);
          else multiFilterStatuses.insert(WatchStatus::Dropped);
        } else if (sidebarSelected == 7) {
          if (multiFilterStatuses.count(WatchStatus::PlanToWatch)) multiFilterStatuses.erase(WatchStatus::PlanToWatch);
          else multiFilterStatuses.insert(WatchStatus::PlanToWatch);
        } else if (sidebarSelected >= 9 && sidebarSelected <= 14) {
          int score = 19 - sidebarSelected;
          if (multiFilterScores.count(score)) multiFilterScores.erase(score);
          else multiFilterScores.insert(score);
        } else if (sidebarSelected == 15) {
          if (multiFilterScores.count(0)) multiFilterScores.erase(0);
          else multiFilterScores.insert(0);
        }
        refreshViews();
        return true;
      }
      if (event == Event::Return) {
        multiFilterStatuses.clear();
        multiFilterScores.clear();
        refreshViews();
        focusedCard = FocusedCard::Catalog;
        catalogMenu->TakeFocus();
        return true;
      }
    } else if (focusedCard == FocusedCard::Details) {
      if (event == Event::Escape) {
        focusedCard = FocusedCard::Catalog;
        appMode = AppMode::Nor;
        return true;
      }

      if (event == Event::Character('j') || event == Event::ArrowDown) {
        if (detailsCursorIndex < 4) detailsCursorIndex++;
        return true;
      }
      if (event == Event::Character('k') || event == Event::ArrowUp) {
        if (detailsCursorIndex > 0) detailsCursorIndex--;
        return true;
      }

      if (!displayedEntries.empty()) {
        auto& e = displayedEntries[catalogSelected];
        if (event == Event::Character('h') || event == Event::ArrowLeft) {
          if (detailsCursorIndex == 0) mutateStatus(e, -1);
          else if (detailsCursorIndex == 1) mutateRating(e, -1);
          else if (detailsCursorIndex == 2) mutateProgress(e, -1);
          return true;
        }
        if (event == Event::Character('l') || event == Event::ArrowRight) {
          if (detailsCursorIndex == 0) mutateStatus(e, 1);
          else if (detailsCursorIndex == 1) mutateRating(e, 1);
          else if (detailsCursorIndex == 2) mutateProgress(e, 1);
          return true;
        }
        if (event == Event::Return || event == Event::Character('e')) {
          if (detailsCursorIndex == 3) {
            inspectorTitleBuffer = e.title;
            isInspectorEditingTitle = true;
            inspectorTitleInput->TakeFocus();
            statusMessage = "Editing title. Press [ret] or [Esc] to commit.";
            return true;
          }
          if (detailsCursorIndex == 4) {
            inspectorNotesBuffer = e.notes;
            isInspectorEditingNotes = true;
            inspectorNotesInput->TakeFocus();
            statusMessage = "Editing notes. Press [ret] or [Esc] to commit.";
            return true;
          }
        }
      }
    }

    if (event == Event::Character('/')) {
      appMode = AppMode::Sch;
      searchInput->TakeFocus();
      statusMessage = "Search title... [ret] Apply  [Esc] Clear";
      return true;
    }

    if (event == Event::Character('a')) {
      openRemoteSearchModal();
      remoteSearchInput->TakeFocus();
      return true;
    }

    if (event == Event::Character('d')) {
      if (!visualSelectedIds.empty()) {
        for (auto id : visualSelectedIds) {
          db_.removeEntry((currentTab == ViewTab::Manga ? MediaType::Manga : MediaType::Anime), id);
        }
        visualSelectedIds.clear();
        appMode = AppMode::Nor;
        db_.saveAll();
        refreshViews();
        statusMessage = "Deleted selected entries.";
      } else if (!displayedEntries.empty() && catalogSelected >= 0 && catalogSelected < static_cast<int>(displayedEntries.size())) {
        pendingDeleteId = displayedEntries[catalogSelected].id;
        statusMessage = "Delete '" + displayedEntries[catalogSelected].title + "'? Confirm [y/N]";
      }
      return true;
    }

    if (event == Event::Character('u')) { performUndo(); refreshViews(); return true; }
    if (event == Event::Character('U') || event == Event::Special("\x12")) { performRedo(); refreshViews(); return true; }

    if (event == Event::Character('o')) {
      if (!displayedEntries.empty() && catalogSelected >= 0 && catalogSelected < static_cast<int>(displayedEntries.size())) {
        openInBrowser(displayedEntries[catalogSelected]);
      }
      return true;
    }

    if (event == Event::Character('y')) {
      if (!displayedEntries.empty() && catalogSelected >= 0 && catalogSelected < static_cast<int>(displayedEntries.size())) {
        copyToClipboard(displayedEntries[catalogSelected].title);
      }
      return true;
    }

    if (event == Event::Character('q')) {
      db_.saveAll();
      screen.ExitLoopClosure()();
      return true;
    }

    return false;
  });

  auto buildOverviewStatisticsSection = [&]() {
    const auto& theme = ThemeRegistry::active();

    auto makeStatSection = [&](MediaType type, const std::string& title, ftxui::Color accentColor) {
      size_t cWatch = db_.countByStatus(type, WatchStatus::Watching);
      size_t cComp  = db_.countByStatus(type, WatchStatus::Completed);
      size_t cHold  = db_.countByStatus(type, WatchStatus::OnHold);
      size_t cDrop  = db_.countByStatus(type, WatchStatus::Dropped);
      size_t cPlan  = db_.countByStatus(type, WatchStatus::PlanToWatch);
      size_t cTotal = db_.getEntries(type).size();

      double days = db_.daysSpent(type);
      double mean = db_.averageScore(type);

      int width = 46;
      Elements barElements;
      if (cTotal == 0) {
        barElements.push_back(text(std::string(width, ' ')) | bgcolor(theme.surface0));
      } else {
        int wWatch = static_cast<int>((cWatch * width) / cTotal);
        int wComp  = static_cast<int>((cComp * width) / cTotal);
        int wHold  = static_cast<int>((cHold * width) / cTotal);
        int wDrop  = static_cast<int>((cDrop * width) / cTotal);
        if (cWatch > 0 && wWatch == 0) wWatch = 1;
        if (cComp > 0 && wComp == 0) wComp = 1;
        if (cHold > 0 && wHold == 0) wHold = 1;
        if (cDrop > 0 && wDrop == 0) wDrop = 1;
        int wSum = wWatch + wComp + wHold + wDrop;
        int wPlan = std::max(0, width - wSum);

        if (wWatch > 0) barElements.push_back(text(std::string(wWatch, ' ')) | bgcolor(theme.green));
        if (wComp > 0)  barElements.push_back(text(std::string(wComp, ' ')) | bgcolor(theme.blue));
        if (wHold > 0)  barElements.push_back(text(std::string(wHold, ' ')) | bgcolor(theme.yellow));
        if (wDrop > 0)  barElements.push_back(text(std::string(wDrop, ' ')) | bgcolor(theme.red));
        if (wPlan > 0)  barElements.push_back(text(std::string(wPlan, ' ')) | bgcolor(theme.overlay0));
      }

      std::stringstream ssMean;
      ssMean << std::fixed << std::setprecision(2) << mean;

      auto statsPane = vbox({
        hbox({
          text(title) | bold | color(accentColor),
          filler(),
          text("All " + title) | color(theme.blue)
        }),
        text(""),
        hbox({
          text("Days: ") | color(theme.subtext),
          text(formatDays(days)) | bold | color(theme.text),
          filler(),
          text("Mean Score: ") | color(theme.subtext),
          text(ssMean.str()) | bold | color(theme.text)
        }),
        text(""),
        hbox(std::move(barElements)),
        text(""),
        hbox({
          vbox({
            hbox({ text("● ") | color(theme.green), text(type == MediaType::Anime ? "Watching" : "Reading") | color(theme.blue), filler(), text(formatCommas(cWatch)) | color(theme.text) }),
            hbox({ text("● ") | color(theme.blue), text("Completed") | color(theme.blue), filler(), text(formatCommas(cComp)) | color(theme.text) }),
            hbox({ text("● ") | color(theme.yellow), text("On-Hold") | color(theme.blue), filler(), text(formatCommas(cHold)) | color(theme.text) }),
            hbox({ text("● ") | color(theme.red), text("Dropped") | color(theme.blue), filler(), text(formatCommas(cDrop)) | color(theme.text) }),
            hbox({ text("● ") | color(theme.overlay0), text(type == MediaType::Anime ? "Plan to Watch" : "Plan to Read") | color(theme.blue), filler(), text(formatCommas(cPlan)) | color(theme.text) })
          }) | size(WIDTH, EQUAL, 23),
          text("   "),
          vbox({
            hbox({ text("Total Entries") | color(theme.subtext), filler(), text(formatCommas(cTotal)) | color(theme.text) }),
            hbox({ text(type == MediaType::Anime ? "Rewatched" : "Reread") | color(theme.subtext), filler(), text(formatCommas(db_.totalRewatched(type))) | color(theme.text) }),
            hbox({ text(type == MediaType::Anime ? "Episodes" : "Chapters") | color(theme.subtext), filler(), text(formatCommas(db_.totalProgress(type))) | color(theme.text) }),
            (type == MediaType::Manga
              ? hbox({ text("Volumes") | color(theme.subtext), filler(), text(formatCommas(db_.totalVolumes(MediaType::Manga))) | color(theme.text) })
              : text(""))
          }) | size(WIDTH, EQUAL, 20)
        })
      }) | size(WIDTH, EQUAL, 48);

      auto recentList = db_.getRecentActivity(type, 3);
      Elements recentBoxes;
      recentBoxes.push_back(hbox({
        text(type == MediaType::Anime ? "Last Anime Updates" : "Last Manga Updates") | bold | color(theme.text),
        filler(),
        text(type == MediaType::Anime ? "Anime History" : "Manga History") | color(theme.blue)
      }));
      recentBoxes.push_back(text(""));

      if (recentList.empty()) {
        recentBoxes.push_back(text(" No recent history logged.") | color(theme.subtext));
      } else {
        for (const auto& act : recentList) {
          float rRatio = (act.totalUnits > 0) ? std::clamp(static_cast<float>(act.progress) / act.totalUnits, 0.0f, 1.0f) : 0.0f;
          int gWidth = 14;
          int filled = static_cast<int>(std::round(rRatio * gWidth));

          auto miniBar = hbox({
            text(std::string(filled, ' ')) | bgcolor(act.status == WatchStatus::Completed ? theme.blue : theme.green),
            text(std::string(std::max(0, gWidth - filled), ' ')) | bgcolor(theme.surface1)
          });

          std::string subline = statusToString(act.status, act.type) + " " + std::to_string(act.progress) + "/" + (act.totalUnits > 0 ? std::to_string(act.totalUnits) : "?") + " · Scored " + (act.score > 0 ? std::to_string(act.score) : "-");

          recentBoxes.push_back(vbox({
            text(clipTitle(act.title, 42)) | bold | color(theme.blue),
            hbox({
              miniBar,
              filler(),
              text(act.timestamp) | color(theme.overlay0)
            }),
            text(subline) | color(theme.subtext),
            text("")
          }));
        }
      }

      auto updatesPane = vbox(std::move(recentBoxes)) | size(WIDTH, EQUAL, 46);

      return hbox({
        statsPane,
        text("    "),
        separator() | color(theme.surface0),
        text("    "),
        updatesPane
      });
    };

    return vbox({
      text("Statistics") | bold | color(theme.text),
      separator() | color(theme.surface0),
      text(""),
      makeStatSection(MediaType::Anime, "Anime Stats", theme.lavender),
      text(""),
      separator() | color(theme.surface0),
      text(""),
      makeStatSection(MediaType::Manga, "Manga Stats", theme.peach),
      filler()
    }) | size(WIDTH, EQUAL, 102) | center | flex;
  };

  // Layman-Accessible Unified About Section
  auto buildAboutSection = [&]() {
    const auto& theme = ThemeRegistry::active();

    auto aboutContent = vbox({
      text(""),
      hbox({
        text(" 󰄛 ") | bold | color(theme.mauve),
        text("Catalist v0.2.0") | bold | color(theme.text),
        filler(),
        text("Minimalist Terminal Media Tracker") | color(theme.subtext)
      }),
      text(""),
      separator() | color(theme.surface0),
      text(""),
      text(" Catalist is a keyboard-driven watchlist manager for your terminal.") | color(theme.text),
      text(" Log watched episodes and read chapters, rate your favorite series,") | color(theme.text),
      text(" and organize your anime and manga libraries with zero clutter.") | color(theme.text),
      text(""),
      hbox({
        vbox({
          text(" Local & Private Library") | bold | color(theme.peach),
          text(""),
          text("  • Total Anime Stored : " + std::to_string(db_.getEntries(MediaType::Anime).size())),
          text("  • Total Manga Stored : " + std::to_string(db_.getEntries(MediaType::Manga).size())),
          text("  • Database Location  : ~/.config/catalist/"),
          text("  • Data Sovereignty   : 100% offline CSV storage, no accounts needed.") | color(theme.green)
        }) | flex,
        text("   "),
        separator() | color(theme.surface0),
        text("   "),
        vbox({
          text(" Online Discovery & Upstream") | bold | color(theme.lavender),
          text(""),
          text("  • Press [a] to search MyAnimeList & AniList interactively."),
          text("  • Automatically fetches episode totals and descriptions."),
          text("  • Reconcile ongoing series totals anytime in the Sync tab."),
          text("  • Create snapshot backups on demand with one keystroke.")
        }) | flex
      }),
      text(""),
      separator() | color(theme.surface0),
      text(""),
      hbox({
        text(" Navigation: ") | bold | color(theme.overlay0),
        text("Press [Tab] or [Shift+Tab] to cycle workspaces  │  Press [:] for Command Palette") | color(theme.subtext),
        filler()
      })
    });

    return vbox({
      aboutContent | border | color(theme.surface0) | size(WIDTH, EQUAL, 88)
    }) | center | flex;
  };

  // Interactive Sync Section
  auto buildSyncSection = [&]() {
    const auto& theme = ThemeRegistry::active();

    auto bDir = Database::getConfigDir() / "backups";
    std::vector<std::string> backupFiles;
    if (std::filesystem::exists(bDir)) {
      for (const auto& entry : std::filesystem::directory_iterator(bDir)) {
        if (entry.is_regular_file()) {
          backupFiles.push_back(entry.path().filename().string());
        }
      }
    }
    std::sort(backupFiles.rbegin(), backupFiles.rend());

    Elements backupElements;
    if (backupFiles.empty()) {
      backupElements.push_back(text("  No snapshots found in backups/.") | color(theme.subtext));
    } else {
      for (size_t i = 0; i < backupFiles.size() && i < 8; ++i) {
        bool selected = (syncActivePanel == 1 && syncBackupCursor == static_cast<int>(i));
        backupElements.push_back(
          hbox({
            text(selected ? " ▌ " : "   ") | color(theme.mauve) | bold,
            text(backupFiles[i]) | (selected ? bold : nothing) | color(selected ? theme.peach : theme.text)
          })
        );
      }
    }

    auto makeActionBtn = [&](int idx, const std::string& iconStr, const std::string& name, const std::string& desc) {
      bool active = (syncActivePanel == 0 && syncActionCursor == idx);
      return vbox({
        hbox({
          text(active ? " ▌ " : "   ") | color(theme.mauve) | bold,
          text(iconStr + " " + name) | (active ? bold : nothing) | color(active ? theme.peach : theme.text)
        }),
        text("     " + desc) | color(theme.subtext),
        text("")
      });
    };

    auto syncStatusText = syncIsRunning.load() ? "● Sync active in background..." : "● Upstream idle and ready.";
    auto syncStatusColor = syncIsRunning.load() ? theme.yellow : theme.green;

    auto panelActions = vbox({
      text(" Upstream Operations") | bold | color(theme.lavender),
      text(""),
      text(" Status: " + std::string(syncStatusText)) | bold | color(syncStatusColor),
      text(""),
      makeActionBtn(0, "󰓦", "Reconcile Ongoing Titles", "Fetches finished totals for ongoing anime from MAL."),
      makeActionBtn(1, "󰆓", "Create Snapshot Backup", "Generates an instant timestamped CSV backup replica."),
      makeActionBtn(2, "󰈞", "Ingest MyAnimeList Archive", "Import an exported official myanimelist.xml file."),
      filler(),
      text(" Press [ret] to trigger selected action.") | color(theme.overlay0)
    }) | border | color(syncActivePanel == 0 ? theme.mauve : theme.surface0)
       | size(WIDTH, EQUAL, 44);

    auto panelBackups = vbox({
      text(" Snapshot Backups Repository") | bold | color(theme.peach),
      text(""),
      text(" Location: ~/.config/catalist/backups/") | color(theme.subtext),
      text(""),
      vbox(std::move(backupElements)) | vscroll_indicator | frame | flex,
      text(""),
      text(" Press [h/l] to switch panels  │  [j/k] to navigate.") | color(theme.overlay0)
    }) | border | color(syncActivePanel == 1 ? theme.mauve : theme.surface0)
       | flex;

    return vbox({
      text(""),
      hbox({ panelActions, text(" "), panelBackups }) | size(WIDTH, EQUAL, 92) | center | flex,
      text("")
    }) | center | flex;
  };

  // Interactive Settings Section
  auto buildSettingsSection = [&]() {
    const auto& theme = ThemeRegistry::active();
    auto themes = ThemeRegistry::getAvailableThemes();

    Elements themeElements;
    for (size_t i = 0; i < themes.size(); ++i) {
      bool active = (config_.theme == themes[i]);
      bool cursorOnThis = (settingsActivePanel == 0 && settingsThemeCursor == static_cast<int>(i));

      std::string marker = active ? (icon::CheckSingle + " ") : (icon::CheckBlank + " ");
      themeElements.push_back(
        hbox({
          text(cursorOnThis ? " ▌ " : "   ") | color(theme.mauve) | bold,
          text(marker) | color(active ? theme.green : theme.overlay0) | bold,
          text(themes[i]) | (active ? bold : nothing) | color(cursorOnThis ? theme.peach : (active ? theme.mauve : theme.text))
        })
      );
    }

    auto makePrefRow = [&](int idx, const std::string& name, const std::string& val) {
      bool active = (settingsActivePanel == 1 && settingsPrefCursor == idx);
      return hbox({
        text(active ? " ▌ " : "   ") | color(theme.mauve) | bold,
        text(name) | size(WIDTH, EQUAL, 22) | (active ? bold : nothing) | color(active ? theme.peach : theme.text),
        text(" " + val) | color(theme.lavender) | bold
      });
    };

    auto panelThemes = vbox({
      text(" Visual Theme Palette") | bold | color(theme.mauve),
      text(""),
      vbox(std::move(themeElements)) | vscroll_indicator | frame | flex,
      text(""),
      text(" Press [ret] to apply selected theme.") | color(theme.overlay0)
    }) | border | color(settingsActivePanel == 0 ? theme.mauve : theme.surface0)
       | size(WIDTH, EQUAL, 38);

    auto panelPrefs = vbox({
      text(" Runtime Preferences") | bold | color(theme.peach),
      text(""),
      makePrefRow(0, "Mouse Support:", config_.mouse ? (icon::CheckSingle + " Enabled") : (icon::CheckBlank + " Disabled")),
      text(""),
      makePrefRow(1, "Default View:", config_.defaultMedia),
      text(""),
      makePrefRow(2, "Default Sort:", config_.defaultSort),
      text(""),
      makePrefRow(3, "Invert Sort:", config_.reverseSort ? (icon::CheckSingle + " Descending (▲)") : (icon::CheckBlank + " Ascending (▼)")),
      text(""),
      text(" Press [ret] or [spc] to cycle options.") | color(theme.overlay0),
      text(""),
      separator() | color(theme.surface0),
      text(""),
      text(" Configuration Store:") | bold | color(theme.lavender),
      text(" ~/.config/catalist/config.toml") | color(theme.subtext),
      text(" Parameters follow ASD-STE100 Simplified Technical English.") | color(theme.overlay0),
      filler()
    }) | border | color(settingsActivePanel == 1 ? theme.mauve : theme.surface0)
       | flex;

    return vbox({
      text(""),
      hbox({ panelThemes, text(" "), panelPrefs }) | size(WIDTH, EQUAL, 92) | center | flex,
      text("")
    }) | center | flex;
  };

  auto mainRenderer = Renderer(globalHandler, [&]() {
    const auto& theme = ThemeRegistry::active();
    size_t aCount = db_.getEntries(MediaType::Anime).size();
    size_t mCount = db_.getEntries(MediaType::Manga).size();

    auto makeTabItem = [&](ViewTab t, const std::string& iconStr, const std::string& title, Box& box, const std::string& countStr = "") {
      bool active = (currentTab == t);
      std::string label = " " + iconStr + " " + title;
      if (!countStr.empty()) label += " (" + countStr + ")";
      label += " ";

      Element el;
      if (active) {
        el = text(label) | bold | color(theme.mauve);
      } else {
        el = text(label) | color(theme.subtext);
      }
      return el | reflect(box);
    };

    auto topCard = hbox({
      makeTabItem(ViewTab::About, icon::AppLogo, "Catalist", tabAboutBox),
      text("│") | color(theme.surface0),
      makeTabItem(ViewTab::Anime, icon::Anime, "Anime", tabAnimeBox, std::to_string(aCount)),
      text("│") | color(theme.surface0),
      makeTabItem(ViewTab::Manga, icon::Manga, "Manga", tabMangaBox, std::to_string(mCount)),
      text("│") | color(theme.surface0),
      makeTabItem(ViewTab::Overview, icon::Stats, "Overview", tabOverviewBox),
      text("│") | color(theme.surface0),
      makeTabItem(ViewTab::Sync, icon::Sync, "Sync", tabSyncBox),
      text("│") | color(theme.surface0),
      makeTabItem(ViewTab::Settings, "󰒓", "Settings", tabSettingsBox),
      filler(),
      text("v0.2.0 ") | color(theme.overlay0)
    }) | border | color(theme.surface0)
       | size(HEIGHT, EQUAL, 3);

    Element contentPane;

    if (currentTab == ViewTab::Overview) {
      contentPane = buildOverviewStatisticsSection();
    } else if (currentTab == ViewTab::About) {
      contentPane = buildAboutSection();
    } else if (currentTab == ViewTab::Sync) {
      contentPane = buildSyncSection();
    } else if (currentTab == ViewTab::Settings) {
      contentPane = buildSettingsSection();
    } else {
      Element details = text("No record selected.") | color(theme.subtext);
      if (!displayedEntries.empty() && catalogSelected >= 0 && catalogSelected < static_cast<int>(displayedEntries.size())) {
        const auto& e = displayedEntries[catalogSelected];
        float ratio = (e.totalUnits > 0) ? std::clamp(static_cast<float>(e.progress) / e.totalUnits, 0.0f, 1.0f) : 0.0f;
        auto gaugeColor = (ratio >= 1.0f || e.status == WatchStatus::Completed) ? theme.green : theme.blue;

        bool onDetails = (focusedCard == FocusedCard::Details);

        auto fieldRow = [&](int idx, const std::string& label, Element content) {
          bool active = onDetails && (detailsCursorIndex == idx);
          std::string prefix = active ? "▌ " : "  ";
          return hbox({
            text(prefix) | color(theme.mauve) | bold,
            text(label) | size(WIDTH, EQUAL, 12) | (active ? bold : nothing) | color(active ? theme.peach : theme.subtext),
            content | flex
          });
        };

        auto typePill = makePill(typeToIcon(e.type) + " " + typeToString(e.type), theme.crust, typeToColor(e.type));
        auto statusPill = makePill(statusToIcon(e.status) + " " + statusToString(e.status, e.type), theme.crust, statusToColor(e.status));

        details = vbox({
          fieldRow(3, "Title:", isInspectorEditingTitle
            ? (inspectorTitleInput->Render() | border | color(theme.peach))
            : (text(e.title) | bold | color(theme.mauve))),
          text(""),
          hbox({
            text("  Type:       ") | color(theme.subtext),
            typePill,
            filler(),
            text("ID #" + std::to_string(e.id) + " ") | color(theme.subtext)
          }),
          text(""),
          fieldRow(0, "Status:", statusPill),
          text(""),
          fieldRow(1, "Rating:", hbox({ text(renderStarMeter(e.score)) | color(theme.yellow), text(" (" + std::to_string(e.score) + "/10)") | color(theme.text) })),
          text(""),
          fieldRow(2, "Progress:", vbox({
            hbox({
              text(std::to_string(e.progress) + " / " + (e.totalUnits > 0 ? std::to_string(e.totalUnits) : "?") + " units") | color(theme.text),
              filler(),
              text(e.totalUnits > 0 ? std::to_string(static_cast<int>(ratio * 100)) + "% " : "Ongoing ") | color(theme.subtext)
            }),
            gauge(ratio) | color(gaugeColor)
          })),
          text(""),
          hbox({ text("  Source MAL: ") | color(theme.subtext), text(e.malId > 0 ? std::to_string(e.malId) : "Local") | color(theme.text) }),
          hbox({ text("  Dates:      ") | color(theme.subtext), text((e.startDate.empty() ? "-" : e.startDate) + " -> " + (e.finishDate.empty() ? "-" : e.finishDate)) | color(theme.text) }),
          text(""),
          fieldRow(4, "Notes:", text("")),
          (isInspectorEditingNotes
            ? (inspectorNotesInput->Render() | border | color(theme.peach))
            : (text(e.notes.empty() ? "  (empty - press e or ret)" : "  " + e.notes) | color(theme.subtext))),
          filler()
        });
      }

      auto makeHeaderCell = [&](const std::string& name, SortKey key, int width, Box& box, bool isFlex = false) {
        bool active = (currentSort == key);
        std::string label = " " + name;
        if (active) label += getSortArrow(key, reverseSort);
        Element cell = text(label);
        if (isFlex) cell = cell | flex;
        else cell = cell | size(WIDTH, EQUAL, width);

        if (active) cell = cell | bold | color(theme.mauve) | underlined;
        else cell = cell | bold | color(theme.subtext);
        return cell | reflect(box);
      };

      auto headerRibbon = hbox({
        text("   ") | size(WIDTH, EQUAL, 3),
        makeHeaderCell("Date", SortKey::Date, 10, headerDateBox),
        text(" "),
        makeHeaderCell("Title", SortKey::Title, 28, headerTitleBox, true),
        text(" "),
        makeHeaderCell("Status", SortKey::Status, 14, headerStatusBox),
        text(" "),
        makeHeaderCell("Rating", SortKey::Score, 12, headerRatingBox),
        text(" "),
        makeHeaderCell("Progress", SortKey::Progress, 12, headerProgressBox),
        text(" ")
      });

      auto sidebarCard = vbox({
        text("  Categories") | bold | color(theme.lavender),
        text(""),
        sidebarMenu->Render() | vscroll_indicator | frame | flex
      }) | border | color(focusedCard == FocusedCard::Sidebar ? theme.mauve : theme.surface0)
         | size(WIDTH, EQUAL, 28);

      // Cleaned Search Card without redundant Add Button
      auto searchCard = hbox({
        text(" " + icon::Search + " ") | color(theme.peach),
        searchInput->Render() | flex,
        text(searchQuery.empty() ? "All entries " : ("Filter: '" + searchQuery + "' ")) | color(theme.subtext),
        text(" ")
      }) | border | color(appMode == AppMode::Sch ? theme.peach : theme.surface0)
         | size(HEIGHT, EQUAL, 3)
         | reflect(searchBarBox);

      auto catalogCard = vbox({
        headerRibbon,
        text(""),
        displayedEntries.empty()
          ? (text(" No matching entries found.") | color(theme.subtext) | center | flex)
          : (catalogMenu->Render() | vscroll_indicator | frame | flex)
      }) | border | color(focusedCard == FocusedCard::Catalog ? theme.mauve : theme.surface0)
         | flex;

      auto detailCard = vbox({
        text("  Details") | bold | color(theme.lavender),
        text(""),
        details | flex
      }) | border | color(focusedCard == FocusedCard::Details ? theme.mauve : theme.surface0)
         | size(WIDTH, EQUAL, 42);

      contentPane = hbox({
        sidebarCard,
        vbox({ searchCard, catalogCard | flex }) | flex,
        detailCard
      });
    }

    // Mode Pill: icon + 3-letter mode strictly
    Element modeBadge;
    if (appMode == AppMode::Cmd) {
      modeBadge = makePill("󰘳 CMD", theme.crust, theme.mauve);
    } else if (appMode == AppMode::Sch) {
      modeBadge = makePill(" SCH", theme.crust, theme.peach);
    } else if (appMode == AppMode::Edt) {
      modeBadge = makePill("󰏫 EDT", theme.crust, theme.yellow);
    } else if (appMode == AppMode::Vis) {
      modeBadge = makePill("󰒅 VIS", theme.crust, theme.lavender);
    } else {
      modeBadge = makePill("󰄛 NOR", theme.crust, theme.green);
    }

    auto hintItem = [&](const std::string& key, const std::string& desc) {
      return hbox({
        text(key) | bold | color(theme.text),
        text(" " + desc) | color(theme.subtext)
      });
    };

    Elements hints;
    if (appMode == AppMode::Nor || appMode == AppMode::Vis) {
      hints = {
        hintItem("Tab", "tabs"),
        text("  "),
        hintItem("h/l", "panes"),
        text("  "),
        hintItem("e/ret", "edit"),
        text("  "),
        hintItem("c/t/s/r/p", "sort"),
        text("  "),
        hintItem("S/R/P", "cycle"),
        text("  "),
        hintItem("+/-", "ep"),
        text("  "),
        hintItem("spc", "select"),
        text("  "),
        hintItem("a", "add"),
        text("  "),
        hintItem("d", "del"),
        text("  "),
        hintItem("/", "find"),
        text("  "),
        hintItem(":", "cmd")
      };
    } else if (appMode == AppMode::Edt) {
      hints = {
        hintItem("j/k", "field"),
        text("  "),
        hintItem("h/l", "adjust"),
        text("  "),
        hintItem("ret", "edit"),
        text("  "),
        hintItem("Esc", "done")
      };
    } else if (appMode == AppMode::Sch) {
      hints = {
        hintItem("ret", "apply"),
        text("  "),
        hintItem("Esc", "clear")
      };
    }

    Element selectionTelemetry = text("");
    if (!visualSelectedIds.empty()) {
      selectionTelemetry = hbox({
        text(" (" + std::to_string(visualSelectedIds.size()) + " selected) ") | bold | color(theme.green),
        text("│ ") | color(theme.surface1)
      });
    }

    // Compression Protection: Truncate status text dynamically so it never squishes hints
    int termWidth = Terminal::Size().dimx;
    int hintsWidth = 76;
    int maxStatusLen = std::max(12, termWidth - hintsWidth - 18);
    std::string safeStatusMessage = statusMessage;
    if (static_cast<int>(safeStatusMessage.size()) > maxStatusLen) {
      safeStatusMessage = safeStatusMessage.substr(0, std::max(0, maxStatusLen - 3)) + "...";
    }

    auto bottomRibbon = hbox({
      text(" "),
      modeBadge,
      text(" │ ") | color(theme.surface1),
      selectionTelemetry,
      hbox(std::move(hints)),
      filler(),
      text(safeStatusMessage) | color(theme.yellow),
      text(" ")
    }) | border | color(theme.surface0)
       | size(HEIGHT, EQUAL, 3);

    auto baseUi = vbox({
      topCard,
      contentPane | flex,
      bottomRibbon
    });

    if (commandPalette_.isOpen()) {
      return dbox({
        baseUi,
        commandPalette_.render(theme)
      });
    }

    if (isOnlineModalOpen) {
      Element resultsList = text(" Type query above and press [ret] to query MyAnimeList.") | color(theme.subtext) | center | flex;
      if (remoteLoading) {
        resultsList = text(" Querying AniList / MyAnimeList...") | bold | color(theme.yellow) | center | flex;
      } else if (!remoteResults.empty()) {
        resultsList = remoteMenu->Render() | vscroll_indicator | frame | flex;
      } else if (!remoteStatusMsg.empty()) {
        resultsList = text(" " + remoteStatusMsg) | color(theme.red) | center | flex;
      }

      auto modalWindow = vbox({
        hbox({
          text("  󰖟 Online Search & Add ") | bold | color(theme.mauve),
          filler(),
          text(" [Esc] Close ") | color(theme.subtext)
        }),
        text(""),
        remoteSearchInput->Render() | border | color(remoteSearchInput->Focused() ? theme.peach : theme.surface0),
        text(""),
        vbox({ resultsList }) | border | color(remoteMenu->Focused() ? theme.mauve : theme.surface0) | size(HEIGHT, EQUAL, 14),
        text(""),
        text(" [ret] Add/Search  │  [Tab] Switch Input/Results  │  [j/k] Select  │  [Esc] Close ") | color(theme.subtext)
      }) | border | color(theme.mauve)
         | bgcolor(theme.base)
         | size(WIDTH, EQUAL, 86)
         | clear_under
         | center;

      return dbox({
        baseUi,
        modalWindow
      });
    }

    return baseUi;
  });

  std::atomic<bool> appRunning{true};
  std::thread tickerThread([&]() {
    while (appRunning.load()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      screenPtr->PostEvent(Event::Custom);
    }
  });

  screen.Loop(mainRenderer);

  appRunning = false;
  if (tickerThread.joinable()) {
    tickerThread.join();
  }
}

} // namespace catalist
