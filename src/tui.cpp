#include "catalist/tui.hpp"
#include "catalist/theme.hpp"
#include "catalist/mal_parser.hpp"
#include "catalist/sync.hpp"

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

static std::string formatSimpleDate(const std::string& dateStr) {
  if (dateStr.empty() || dateStr.find("0000") == 0) {
    return "-       ";
  }
  if (dateStr.size() >= 7) {
    return dateStr.substr(0, 7) + " ";
  }
  std::string padded = dateStr;
  while (padded.size() < 8) padded += " ";
  return padded;
}

// Case-insensitive substring match highlighter
static Element highlightMatches(const std::string& textStr, const std::string& query, bool isRowActive) {
  if (query.empty()) {
    auto el = text(textStr) | color(theme::Text);
    if (isRowActive) el = el | bold;
    return el;
  }

  std::string lowerText = textStr;
  std::string lowerQuery = query;
  std::transform(lowerText.begin(), lowerText.end(), lowerText.begin(), [](unsigned char c) { return std::tolower(c); });
  std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), [](unsigned char c) { return std::tolower(c); });

  size_t lastPos = 0;
  size_t pos = lowerText.find(lowerQuery, 0);

  if (pos == std::string::npos) {
    auto el = text(textStr) | color(theme::Text);
    if (isRowActive) el = el | bold;
    return el;
  }

  Elements parts;
  while (pos != std::string::npos) {
    if (pos > lastPos) {
      auto nonMatch = text(textStr.substr(lastPos, pos - lastPos)) | color(theme::Text);
      if (isRowActive) nonMatch = nonMatch | bold;
      parts.push_back(nonMatch);
    }
    auto matchEl = text(textStr.substr(pos, lowerQuery.size()))
                   | bold
                   | color(theme::Crust)
                   | bgcolor(theme::Yellow);
    parts.push_back(matchEl);
    lastPos = pos + lowerQuery.size();
    pos = lowerText.find(lowerQuery, lastPos);
  }

  if (lastPos < textStr.size()) {
    auto post = text(textStr.substr(lastPos)) | color(theme::Text);
    if (isRowActive) post = post | bold;
    parts.push_back(post);
  }

  return hbox(std::move(parts));
}

AppTui::AppTui(Database& db, AppConfig& config)
    : db_(db), config_(config) {}

void AppTui::run() {
  auto screen = ScreenInteractive::Fullscreen();

  ViewTab currentTab = (config_.defaultMedia == "manga") ? ViewTab::Manga : ViewTab::Anime;
  ActiveTier currentTier = ActiveTier::Catalog;
  EditField activeCellField = EditField::Progress;
  SortKey currentSort = SortKey::Score;
  bool reverseSort = config_.reverseSort;

  int sidebarSelected = 0;
  int catalogSelected = 0;
  std::string searchQuery = "";
  std::string commandBuffer = "";
  std::string inspectorTitleBuffer = "";
  std::string inspectorNotesBuffer = "";
  std::string statusMessage = "Ready.";
  int pendingDeleteId = 0;
  bool commandModeActive = false;

  int marqueeEntryId = -1;
  auto marqueeStartTime = std::chrono::steady_clock::now();

  std::vector<std::string> sidebarLabels;
  std::vector<std::string> catalogIndexLabels;
  std::vector<Entry> displayedEntries;

  // Screen coordinate reflection boxes
  Box tabAnimeBox, tabMangaBox, tabOverviewBox;
  Box headerDateBox, headerTitleBox, headerStatusBox, headerRatingBox, headerProgressBox;
  Box searchBarBox, newEntryBtnBox;
  Box sidebarCardBox, catalogCardBox, detailCardBox;
  std::vector<Box> rowBoxes;
  std::vector<Box> sidebarBoxes;

  // Detail card interactive reflection boxes
  Box detailsTitleBox, detailsIdBox, detailsMalIdBox, detailsDeleteBox;
  Box detailsStatusBox, detailsRatingBox;
  Box detailsProgressMinusBox, detailsProgressPlusBox, detailsProgressGaugeBox;
  Box detailsStartBox, detailsFinishBox, detailsNotesBox;

  auto boxContains = [](const Box& b, int x, int y) {
    return (b.x_min != 0 || b.x_max != 0 || b.y_min != 0 || b.y_max != 0) &&
           x >= b.x_min && x <= b.x_max &&
           y >= b.y_min && y <= b.y_max;
  };

  auto copyToClipboard = [&](const std::string& str) {
    if (str.empty()) return;

    FILE* pipe = popen("wl-copy 2>/dev/null", "w");
    if (!pipe) {
      pipe = popen("xclip -selection clipboard 2>/dev/null", "w");
    }
    if (!pipe) {
      pipe = popen("pbcopy 2>/dev/null", "w");
    }
    if (pipe) {
      fwrite(str.data(), 1, str.size(), pipe);
      pclose(pipe);
    }

    // ANSI OSC 52 fallback for Wayland terminal emulators (Ghostty)
    static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string b64 = "";
    int val = 0, valb = -6;
    for (unsigned char c : str) {
      val = (val << 8) + c;
      valb += 8;
      while (valb >= 0) {
        b64.push_back(b64_table[(val >> valb) & 0x3F]);
        valb -= 6;
      }
    }
    if (valb > -6) b64.push_back(b64_table[((val << 8) >> (valb + 8)) & 0x3F]);
    while (b64.size() % 4) b64.push_back('=');
    std::cout << "\033]52;c;" << b64 << "\007" << std::flush;

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

  auto getSortArrow = [](SortKey key, bool reverse) -> std::string {
    bool desc = false;
    switch (key) {
      case SortKey::Score:
      case SortKey::Progress:
      case SortKey::Date:
        desc = !reverse;
        break;
      case SortKey::Title:
      case SortKey::Status:
      case SortKey::Id:
        desc = reverse;
        break;
    }
    return desc ? " ▼" : " ▲";
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

    if (act.type == HistoryAction::Mutate) {
      db_.updateEntry(act.before);
    } else if (act.type == HistoryAction::Delete) {
      db_.addEntry(act.before);
    } else if (act.type == HistoryAction::Add) {
      db_.removeEntry(act.media, act.after.id);
    }
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

    if (act.type == HistoryAction::Mutate) {
      db_.updateEntry(act.after);
    } else if (act.type == HistoryAction::Delete) {
      db_.removeEntry(act.media, act.before.id);
    } else if (act.type == HistoryAction::Add) {
      db_.addEntry(act.after);
    }
    db_.saveAll();
    statusMessage = "Action redone.";
  };

  auto refreshViews = [&]() {
    MediaType media = (currentTab == ViewTab::Manga) ? MediaType::Manga : MediaType::Anime;

    sidebarLabels = {
      icon::Library    + " All Media    (" + std::to_string(db_.getEntries(media).size()) + ")",
      icon::Watching   + (media == MediaType::Manga ? " Reading      (" : " Watching     (") + std::to_string(db_.countByStatus(media, WatchStatus::Watching)) + ")",
      icon::Completed  + " Completed    (" + std::to_string(db_.countByStatus(media, WatchStatus::Completed)) + ")",
      icon::OnHold     + " On-Hold      (" + std::to_string(db_.countByStatus(media, WatchStatus::OnHold)) + ")",
      icon::Dropped    + " Dropped      (" + std::to_string(db_.countByStatus(media, WatchStatus::Dropped)) + ")",
      icon::Plan       + " Planned      (" + std::to_string(db_.countByStatus(media, WatchStatus::PlanToWatch)) + ")",
      "  Ratings",
      renderStarMeter(10) + " (" + std::to_string(db_.countByScore(media, 10)) + ")",
      renderStarMeter(9)  + " (" + std::to_string(db_.countByScore(media, 9)) + ")",
      renderStarMeter(8)  + " (" + std::to_string(db_.countByScore(media, 8)) + ")",
      renderStarMeter(7)  + " (" + std::to_string(db_.countByScore(media, 7)) + ")",
      renderStarMeter(6)  + " (" + std::to_string(db_.countByScore(media, 6)) + ")",
      renderStarMeter(5)  + " (" + std::to_string(db_.countByScore(media, 5)) + ")",
      renderStarMeter(0)  + " (" + std::to_string(db_.countByScore(media, 0)) + ")"
    };

    std::optional<WatchStatus> filterStatus = std::nullopt;
    int scoreFilter = -1;

    switch (sidebarSelected) {
      case 1: filterStatus = WatchStatus::Watching; break;
      case 2: filterStatus = WatchStatus::Completed; break;
      case 3: filterStatus = WatchStatus::OnHold; break;
      case 4: filterStatus = WatchStatus::Dropped; break;
      case 5: filterStatus = WatchStatus::PlanToWatch; break;
      case 7: scoreFilter = 10; break;
      case 8: scoreFilter = 9; break;
      case 9: scoreFilter = 8; break;
      case 10: scoreFilter = 7; break;
      case 11: scoreFilter = 6; break;
      case 12: scoreFilter = 5; break;
      case 13: scoreFilter = 0; break;
      default: break;
    }

    displayedEntries = db_.filterEntries(media, filterStatus, scoreFilter, searchQuery, currentSort, reverseSort);

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

  auto createNewEntry = [&]() {
    MediaType media = (currentTab == ViewTab::Manga) ? MediaType::Manga : MediaType::Anime;
    Entry e;
    e.type = media;
    e.title = "New " + std::string(media == MediaType::Anime ? "Anime" : "Manga");
    e.status = WatchStatus::Watching;
    e.score = 0;
    e.progress = 0;
    e.totalUnits = 0;

    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d");
    e.startDate = ss.str();
    e.updatedAt = ss.str();

    int newId = db_.addEntry(e);
    recordAction(HistoryAction::Add, media, Entry{}, e);
    db_.saveAll();
    refreshViews();

    for (size_t i = 0; i < displayedEntries.size(); ++i) {
      if (displayedEntries[i].id == newId) {
        catalogSelected = static_cast<int>(i);
        break;
      }
    }

    currentTier = ActiveTier::CellEdit;
    activeCellField = EditField::Progress;
    statusMessage = "Created new entry. [h/l/j/k] to edit, [ret] for notes/title.";
  };

  MenuOption sidebarOpt;
  sidebarOpt.on_change = [&]() {
    refreshViews();
  };
  sidebarOpt.entries_option.transform = [&](const EntryState& state) {
    int idx = -1;
    for (size_t i = 0; i < sidebarLabels.size(); ++i) {
      if (sidebarLabels[i] == state.label) {
        idx = static_cast<int>(i);
        break;
      }
    }

    if (idx == 6 || state.label == "  Ratings") {
      return text("  RATINGS") | bold | color(theme::Overlay0);
    }
    bool focused = state.focused && (currentTier == ActiveTier::Sidebar);
    Element el;
    if (focused) {
      el = hbox({
        text("▌ ") | color(theme::Mauve) | bold,
        text(state.label) | bold | color(theme::Text)
      }) | bgcolor(theme::Surface0) | focus;
    } else if (state.active) {
      el = hbox({
        text("▎ ") | color(theme::Mauve),
        text(state.label) | bold | color(theme::Text)
      }) | bgcolor(theme::Surface0);
    } else {
      el = hbox({
        text("  "),
        text(state.label) | color(theme::Text)
      });
    }

    if (idx >= 0 && idx < static_cast<int>(sidebarBoxes.size())) {
      el = el | reflect(sidebarBoxes[idx]);
    }
    return el;
  };
  auto sidebarMenu = Menu(&sidebarLabels, &sidebarSelected, sidebarOpt);

  InputOption searchOpt;
  searchOpt.transform = [](InputState state) {
    Element el = state.element;
    if (state.is_placeholder) {
      el = el | color(theme::Overlay0);
    } else {
      el = el | color(theme::Text);
    }
    if (state.focused) {
      el = el | color(theme::Peach);
    }
    return el;
  };
  auto searchInput = Input(&searchQuery, "Click or press '/' to search title...", searchOpt);

  MenuOption catalogOpt;
  catalogOpt.entries_option.transform = [&](const EntryState& state) {
    int idx = 0;
    try {
      idx = std::stoi(state.label);
    } catch (...) {
      return text("");
    }
    if (idx < 0 || idx >= static_cast<int>(displayedEntries.size())) {
      return text("");
    }
    const auto& e = displayedEntries[idx];
    bool isRowActive = (idx == catalogSelected) && (currentTier == ActiveTier::Catalog || currentTier == ActiveTier::CellEdit);

    size_t titleGlyphs = 0;
    for (size_t i = 0; i < e.title.size(); ) {
      unsigned char c = static_cast<unsigned char>(e.title[i]);
      if ((c & 0x80) == 0) i += 1;
      else if ((c & 0xE0) == 0xC0) i += 2;
      else if ((c & 0xF0) == 0xE0) i += 3;
      else i += 4;
      titleGlyphs++;
    }

    std::string displayTitle = e.title;
    if (idx == catalogSelected && titleGlyphs > 26) {
      auto now = std::chrono::steady_clock::now();
      if (e.id != marqueeEntryId) {
        marqueeEntryId = e.id;
        marqueeStartTime = now;
      }
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - marqueeStartTime).count();
      if (elapsed > 800) {
        size_t offset = static_cast<size_t>((elapsed - 800) / 160);
        displayTitle = clipTitle(e.title + "    ──    " + e.title, 26, offset);
      } else {
        displayTitle = clipTitle(e.title, 26, 0);
      }
    } else {
      displayTitle = clipTitle(e.title, 26, 0);
    }

    // Dynamic case-insensitive substring highlighting in Title
    auto titleElement = hbox({
      text(" " + typeToIcon(e.type) + " "),
      highlightMatches(displayTitle, searchQuery, isRowActive)
    }) | flex;

    auto statusStyle = (currentTier == ActiveTier::CellEdit && isRowActive && activeCellField == EditField::Status)
      ? (bold | color(theme::Crust) | bgcolor(statusToColor(e.status)))
      : (color(statusToColor(e.status)));

    auto ratingStyle = (currentTier == ActiveTier::CellEdit && isRowActive && activeCellField == EditField::Rating)
      ? (bold | color(theme::Crust) | bgcolor(theme::Yellow))
      : (color(theme::Yellow));

    auto progressStyle = (currentTier == ActiveTier::CellEdit && isRowActive && activeCellField == EditField::Progress)
      ? (bold | color(theme::Crust) | bgcolor(theme::Lavender))
      : (color(theme::Lavender));

    auto row = hbox({
      text(isRowActive ? "▌ " : "  ") | color(theme::Mauve) | bold,
      text(" " + formatSimpleDate(e.startDate)) | size(WIDTH, EQUAL, 10) | color(theme::Subtext),
      text(" "),
      titleElement,
      text(" "),
      text(" " + statusToIcon(e.status) + " " + statusToString(e.status, e.type)) | size(WIDTH, EQUAL, 14) | statusStyle,
      text(" "),
      text(" " + renderStarMeter(e.score)) | size(WIDTH, EQUAL, 12) | ratingStyle,
      text(" "),
      text(" " + icon::Gauge + " " + std::to_string(e.progress) + "/" + (e.totalUnits > 0 ? std::to_string(e.totalUnits) : "?")) | size(WIDTH, EQUAL, 12) | progressStyle,
      text(" ")
    });

    if (isRowActive) {
      // | focus tells FTXUI's frame to follow and scroll the active row
      row = row | bgcolor(theme::Surface0) | focus;
    }

    if (idx >= 0 && idx < static_cast<int>(rowBoxes.size())) {
      row = row | reflect(rowBoxes[idx]);
    }
    return row;
  };

  auto catalogMenu = Menu(&catalogIndexLabels, &catalogSelected, catalogOpt);
  auto inspectorTitleInput = Input(&inspectorTitleBuffer, "Title...");
  auto inspectorNotesInput = Input(&inspectorNotesBuffer, "Write notes here...");

  auto catalogContainer = Container::Vertical({
    searchInput,
    catalogMenu
  });

  auto inspectorContainer = Container::Vertical({
    inspectorTitleInput,
    inspectorNotesInput
  });

  auto mainContainer = Container::Horizontal({
    sidebarMenu,
    catalogContainer,
    inspectorContainer
  });

  auto mutateEditField = [&](Entry& e, int dir) {
    Entry before = e;
    if (activeCellField == EditField::Rating) {
      e.score = std::clamp(e.score + dir, 0, 10);
      statusMessage = "Rating: " + renderStarMeter(e.score);
    } else if (activeCellField == EditField::Progress) {
      e.progress = std::max(0, e.progress + dir);
      if (e.totalUnits > 0 && e.progress >= e.totalUnits) {
        e.progress = e.totalUnits;
        e.status = WatchStatus::Completed;
      }
      statusMessage = "Progress: " + std::to_string(e.progress) + " units";
    } else if (activeCellField == EditField::Status) {
      switch (e.status) {
        case WatchStatus::Watching:    e.status = (dir > 0) ? WatchStatus::Completed : WatchStatus::PlanToWatch; break;
        case WatchStatus::Completed:   e.status = (dir > 0) ? WatchStatus::OnHold : WatchStatus::Watching; break;
        case WatchStatus::OnHold:      e.status = (dir > 0) ? WatchStatus::Dropped : WatchStatus::Completed; break;
        case WatchStatus::Dropped:     e.status = (dir > 0) ? WatchStatus::PlanToWatch : WatchStatus::OnHold; break;
        case WatchStatus::PlanToWatch: e.status = (dir > 0) ? WatchStatus::Watching : WatchStatus::Dropped; break;
      }
      statusMessage = "Status: " + statusToString(e.status, e.type);
    }
    recordAction(HistoryAction::Mutate, e.type, before, e);
    db_.updateEntry(e);
    db_.saveAll();

    if (catalogSelected >= 0 && catalogSelected < static_cast<int>(displayedEntries.size())) {
      displayedEntries[catalogSelected] = e;
    }
  };

  auto executeCommand = [&](const std::string& cmdLine) {
    if (cmdLine.empty()) return;
    std::stringstream ss(cmdLine);
    std::string verb;
    ss >> verb;

    if (verb == "q" || verb == "quit") {
      db_.saveAll();
      screen.ExitLoopClosure()();
    } else if (verb == "w" || verb == "write") {
      db_.saveAll();
      statusMessage = "Saved watchlists to disk.";
    } else if (verb == "wq") {
      db_.saveAll();
      screen.ExitLoopClosure()();
    } else if (verb == "new" || verb == "add") {
      createNewEntry();
    } else if (verb == "undo" || verb == "u") {
      performUndo();
      refreshViews();
    } else if (verb == "redo") {
      performRedo();
      refreshViews();
    } else if (verb == "open" || verb == "mal") {
      if (!displayedEntries.empty()) openInBrowser(displayedEntries[catalogSelected]);
    } else if (verb == "sort") {
      std::string key, order;
      ss >> key >> order;
      if (key == "date" || key == "chrono") currentSort = SortKey::Date;
      else if (key == "title") currentSort = SortKey::Title;
      else if (key == "status") currentSort = SortKey::Status;
      else if (key == "score" || key == "rating") currentSort = SortKey::Score;
      else if (key == "progress") currentSort = SortKey::Progress;
      else if (key == "id" || key == "mal") currentSort = SortKey::Id;

      if (!order.empty()) reverseSort = (order == "desc" || order == "reverse");
      refreshViews();
      statusMessage = "Sorted by: " + key;
    } else if (verb == "backup") {
      db_.createBackupSnapshot();
      statusMessage = "Backup created in ~/.config/catalist/backups/";
    } else if (verb == "sync") {
      statusMessage = "Syncing metadata...";
      std::string msg;
      MetadataSync::syncOngoing(db_, msg);
      refreshViews();
      statusMessage = msg;
    } else {
      statusMessage = "Unknown command: '" + verb + "'";
    }
  };

  auto globalHandler = CatchEvent(mainContainer, [&](Event event) {
    if (event == Event::Custom) {
      return true;
    }

    if (!config_.mouse && event.is_mouse()) {
      return true;
    }

    // Comprehensive Mouse Event Interactivity
    if (event.is_mouse()) {
      auto mouse = event.mouse();
      int mx = mouse.x;
      int my = mouse.y;

      // Mouse Wheel Scrolling
      if (mouse.button == Mouse::WheelUp) {
        if (boxContains(sidebarCardBox, mx, my)) {
          if (sidebarSelected > 0) {
            sidebarSelected--;
            if (sidebarSelected == 6) sidebarSelected = 5;
            refreshViews();
          }
          return true;
        }
        if (catalogSelected > 0) {
          catalogSelected = std::max(0, catalogSelected - 1);
          marqueeEntryId = displayedEntries[catalogSelected].id;
          marqueeStartTime = std::chrono::steady_clock::now();
        }
        return true;
      }
      if (mouse.button == Mouse::WheelDown) {
        if (boxContains(sidebarCardBox, mx, my)) {
          if (sidebarSelected < static_cast<int>(sidebarLabels.size()) - 1) {
            sidebarSelected++;
            if (sidebarSelected == 6) sidebarSelected = 7;
            refreshViews();
          }
          return true;
        }
        if (!displayedEntries.empty() && catalogSelected < static_cast<int>(displayedEntries.size()) - 1) {
          catalogSelected = std::min(static_cast<int>(displayedEntries.size()) - 1, catalogSelected + 1);
          marqueeEntryId = displayedEntries[catalogSelected].id;
          marqueeStartTime = std::chrono::steady_clock::now();
        }
        return true;
      }

      if (mouse.button == Mouse::Left && mouse.motion == Mouse::Pressed) {
        // Tab switching
        if (boxContains(tabAnimeBox, mx, my)) {
          currentTab = ViewTab::Anime;
          catalogSelected = 0;
          refreshViews();
          return true;
        }
        if (boxContains(tabMangaBox, mx, my)) {
          currentTab = ViewTab::Manga;
          catalogSelected = 0;
          refreshViews();
          return true;
        }
        if (boxContains(tabOverviewBox, mx, my)) {
          currentTab = ViewTab::Overview;
          refreshViews();
          return true;
        }

        // Search card & [+ New] button
        if (boxContains(newEntryBtnBox, mx, my)) {
          createNewEntry();
          return true;
        }
        if (boxContains(searchBarBox, mx, my)) {
          searchInput->TakeFocus();
          currentTier = ActiveTier::Search;
          statusMessage = "Type search query...";
          return true;
        }

        // Header column sort triggers (in chronological ribbon order)
        if (boxContains(headerDateBox, mx, my)) {
          if (currentSort == SortKey::Date) reverseSort = !reverseSort;
          else { currentSort = SortKey::Date; reverseSort = false; }
          refreshViews();
          statusMessage = "Sorted by Date" + getSortArrow(SortKey::Date, reverseSort);
          return true;
        }
        if (boxContains(headerTitleBox, mx, my)) {
          if (currentSort == SortKey::Title) reverseSort = !reverseSort;
          else { currentSort = SortKey::Title; reverseSort = false; }
          refreshViews();
          statusMessage = "Sorted by Title" + getSortArrow(SortKey::Title, reverseSort);
          return true;
        }
        if (boxContains(headerStatusBox, mx, my)) {
          if (currentSort == SortKey::Status) reverseSort = !reverseSort;
          else { currentSort = SortKey::Status; reverseSort = false; }
          refreshViews();
          statusMessage = "Sorted by Status" + getSortArrow(SortKey::Status, reverseSort);
          return true;
        }
        if (boxContains(headerRatingBox, mx, my)) {
          if (currentSort == SortKey::Score) reverseSort = !reverseSort;
          else { currentSort = SortKey::Score; reverseSort = false; }
          refreshViews();
          statusMessage = "Sorted by Rating" + getSortArrow(SortKey::Score, reverseSort);
          return true;
        }
        if (boxContains(headerProgressBox, mx, my)) {
          if (currentSort == SortKey::Progress) reverseSort = !reverseSort;
          else { currentSort = SortKey::Progress; reverseSort = false; }
          refreshViews();
          statusMessage = "Sorted by Progress" + getSortArrow(SortKey::Progress, reverseSort);
          return true;
        }

        // Sidebar categories and star rating clicks
        for (size_t i = 0; i < sidebarLabels.size() && i < sidebarBoxes.size(); ++i) {
          if (boxContains(sidebarBoxes[i], mx, my)) {
            if (i == 6) return true;
            sidebarSelected = static_cast<int>(i);
            refreshViews();
            catalogMenu->TakeFocus();
            currentTier = ActiveTier::Catalog;
            statusMessage = "Filtered by: " + sidebarLabels[i];
            return true;
          }
        }

        // Interactive Detail Card Clicks
        if (!displayedEntries.empty() && catalogSelected >= 0 && catalogSelected < static_cast<int>(displayedEntries.size())) {
          auto& e = displayedEntries[catalogSelected];

          // 1. Delete button
          if (boxContains(detailsDeleteBox, mx, my)) {
            pendingDeleteId = e.id;
            statusMessage = "Delete '" + e.title + "'? Confirm [y/N]";
            return true;
          }

          // 2. Progress buttons [-] and [+]
          if (boxContains(detailsProgressMinusBox, mx, my)) {
            mutateEditField(e, -1);
            return true;
          }
          if (boxContains(detailsProgressPlusBox, mx, my)) {
            mutateEditField(e, 1);
            return true;
          }

          // 3. Direct scrub on progress gauge
          if (boxContains(detailsProgressGaugeBox, mx, my) && e.totalUnits > 0) {
            int gw = detailsProgressGaugeBox.x_max - detailsProgressGaugeBox.x_min;
            if (gw > 0) {
              float ratio = std::clamp(static_cast<float>(mx - detailsProgressGaugeBox.x_min) / gw, 0.0f, 1.0f);
              Entry before = e;
              e.progress = static_cast<int>(std::round(ratio * e.totalUnits));
              if (e.progress >= e.totalUnits) e.status = WatchStatus::Completed;
              recordAction(HistoryAction::Mutate, e.type, before, e);
              db_.updateEntry(e);
              db_.saveAll();
              displayedEntries[catalogSelected] = e;
              statusMessage = "Progress set to " + std::to_string(e.progress);
              return true;
            }
          }

          // 4. Clickable Star Rating Cluster
          if (boxContains(detailsRatingBox, mx, my)) {
            int relX = mx - detailsRatingBox.x_min - 9;
            int clickedScore = std::clamp((relX / 2 + 1) * 2, 0, 10);
            if (relX <= 0) clickedScore = 0;
            Entry before = e;
            e.score = clickedScore;
            recordAction(HistoryAction::Mutate, e.type, before, e);
            db_.updateEntry(e);
            db_.saveAll();
            displayedEntries[catalogSelected] = e;
            statusMessage = "Rating set to " + std::to_string(e.score) + "/10";
            return true;
          }

          // 5. Click Status Badge to cycle
          if (boxContains(detailsStatusBox, mx, my)) {
            activeCellField = EditField::Status;
            mutateEditField(e, 1);
            return true;
          }

          // 6. Click Title or Notes field to edit in Inspector
          if (boxContains(detailsTitleBox, mx, my) || boxContains(detailsNotesBox, mx, my)) {
            inspectorTitleBuffer = e.title;
            inspectorNotesBuffer = e.notes;
            if (boxContains(detailsTitleBox, mx, my)) {
              inspectorTitleInput->TakeFocus();
            } else {
              inspectorNotesInput->TakeFocus();
            }
            currentTier = ActiveTier::Inspector;
            statusMessage = "Editing details. [ret] to commit.";
            return true;
          }

          // 7. Copy on click for metadata fields
          if (boxContains(detailsIdBox, mx, my) || boxContains(detailsMalIdBox, mx, my)) {
            copyToClipboard(std::to_string(e.malId > 0 ? e.malId : e.id));
            return true;
          }
          if (boxContains(detailsStartBox, mx, my)) {
            copyToClipboard(e.startDate);
            return true;
          }
          if (boxContains(detailsFinishBox, mx, my)) {
            copyToClipboard(e.finishDate);
            return true;
          }

          // 8. General click anywhere on detailCard copies title to clipboard
          if (boxContains(detailCardBox, mx, my)) {
            copyToClipboard(e.title);
            return true;
          }
        }
      }

      // Auto-highlight entry on hover (restricted strictly to mouse MOTION to avoid clobbering wheel)
      if (mouse.button == Mouse::None && currentTier == ActiveTier::Catalog) {
        for (int i = 0; i < static_cast<int>(displayedEntries.size()) && i < static_cast<int>(rowBoxes.size()); ++i) {
          if (boxContains(rowBoxes[i], mx, my)) {
            if (catalogSelected != i) {
              catalogSelected = i;
              marqueeEntryId = displayedEntries[i].id;
              marqueeStartTime = std::chrono::steady_clock::now();
            }
            break;
          }
        }
      }
    }

    if (pendingDeleteId > 0) {
      if (event == Event::Character('y') || event == Event::Character('Y')) {
        MediaType media = (currentTab == ViewTab::Manga) ? MediaType::Manga : MediaType::Anime;
        auto optEntry = db_.getById(media, pendingDeleteId);
        if (optEntry.has_value()) {
          recordAction(HistoryAction::Delete, media, optEntry.value(), Entry{});
        }
        db_.removeEntry(media, pendingDeleteId);
        pendingDeleteId = 0;
        db_.saveAll();
        refreshViews();
        statusMessage = "Record deleted. Press 'u' to undo.";
        return true;
      } else {
        pendingDeleteId = 0;
        statusMessage = "Deletion canceled.";
        return true;
      }
    }

    // SEARCH BAR ISOLATION: Highest priority, zero hotkey leakage
    if (currentTier == ActiveTier::Search || searchInput->Focused()) {
      if (event == Event::Escape) {
        searchQuery = "";
        refreshViews();
        catalogMenu->TakeFocus();
        currentTier = ActiveTier::Catalog;
        statusMessage = "Search cleared.";
        return true;
      }
      if (event == Event::Return || event == Event::ArrowDown) {
        refreshViews();
        catalogMenu->TakeFocus();
        currentTier = ActiveTier::Catalog;
        statusMessage = searchQuery.empty() ? "Filter cleared." : ("Filtered: '" + searchQuery + "' (" + std::to_string(displayedEntries.size()) + " matches)");
        return true;
      }
      searchInput->OnEvent(event);
      refreshViews();
      return true;
    }

    // COMMAND Mode
    if (commandModeActive) {
      if (event == Event::Escape) {
        commandModeActive = false;
        commandBuffer = "";
        statusMessage = "Command canceled.";
        return true;
      }
      if (event == Event::Return) {
        commandModeActive = false;
        executeCommand(commandBuffer);
        commandBuffer = "";
        return true;
      }
      if (event == Event::Backspace) {
        if (!commandBuffer.empty()) commandBuffer.pop_back();
        return true;
      }
      if (event.is_character()) {
        commandBuffer += event.character();
        return true;
      }
      return true;
    }

    // Undo / Redo
    if (event == Event::Special("\x1a") || event == Event::Character('u')) {
      if (currentTier == ActiveTier::Catalog) {
        performUndo();
        refreshViews();
        return true;
      }
    }
    if (event == Event::Special("\x19") || event == Event::Special("\x12")) {
      if (currentTier == ActiveTier::Catalog) {
        performRedo();
        refreshViews();
        return true;
      }
    }

    // Inspector Note & Title Editor
    if (currentTier == ActiveTier::Inspector) {
      if (event == Event::Tab || (event == Event::Return && inspectorTitleInput->Focused())) {
        inspectorNotesInput->TakeFocus();
        return true;
      }
      if (event == Event::Escape || (event == Event::Return && inspectorNotesInput->Focused())) {
        if (!displayedEntries.empty()) {
          auto& e = displayedEntries[catalogSelected];
          Entry before = e;
          if (!inspectorTitleBuffer.empty()) e.title = inspectorTitleBuffer;
          e.notes = inspectorNotesBuffer;
          recordAction(HistoryAction::Mutate, e.type, before, e);
          db_.updateEntry(e);
          db_.saveAll();
        }
        catalogMenu->TakeFocus();
        currentTier = ActiveTier::Catalog;
        refreshViews();
        statusMessage = "Details saved.";
        return true;
      }
      if (inspectorTitleInput->Focused()) {
        inspectorTitleInput->OnEvent(event);
      } else {
        inspectorNotesInput->OnEvent(event);
      }
      return true;
    }

    // Tier 3: Cell Edit Mode
    if (currentTier == ActiveTier::CellEdit) {
      if (!displayedEntries.empty()) {
        auto& e = displayedEntries[catalogSelected];
        if (event == Event::Character('h') || event == Event::ArrowLeft) {
          switch (activeCellField) {
            case EditField::Status:   activeCellField = EditField::Progress; break;
            case EditField::Rating:   activeCellField = EditField::Status; break;
            case EditField::Progress: activeCellField = EditField::Rating; break;
          }
          return true;
        }
        if (event == Event::Character('l') || event == Event::ArrowRight) {
          switch (activeCellField) {
            case EditField::Status:   activeCellField = EditField::Rating; break;
            case EditField::Rating:   activeCellField = EditField::Progress; break;
            case EditField::Progress: activeCellField = EditField::Status; break;
          }
          return true;
        }
        if (event == Event::Character('j') || event == Event::ArrowDown || event == Event::Character('-')) {
          mutateEditField(e, -1);
          return true;
        }
        if (event == Event::Character('k') || event == Event::ArrowUp || event == Event::Character('+') || event == Event::Character('=')) {
          mutateEditField(e, 1);
          return true;
        }
        if (event == Event::Return) {
          inspectorTitleBuffer = e.title;
          inspectorNotesBuffer = e.notes;
          inspectorTitleInput->TakeFocus();
          currentTier = ActiveTier::Inspector;
          statusMessage = "Editing title/notes. [ret] to save.";
          return true;
        }
        if (event == Event::Escape || event == Event::Backspace) {
          currentTier = ActiveTier::Catalog;
          refreshViews();
          statusMessage = "Saved changes.";
          return true;
        }
      }
      return false;
    }

    // Catalog & Sidebar Normal Navigation
    if (currentTier == ActiveTier::Catalog || currentTier == ActiveTier::Sidebar) {
      if (event == Event::Character('q')) {
        db_.saveAll();
        screen.ExitLoopClosure()();
        return true;
      }

      if (event == Event::Character(':')) {
        commandModeActive = true;
        commandBuffer = "";
        return true;
      }

      // Catalog Sorting Hotkeys: Ordered chronologically as [c, t, s, r, p]
      if (currentTier == ActiveTier::Catalog) {
        if (event == Event::Character('c')) {
          if (currentSort == SortKey::Date) reverseSort = !reverseSort;
          else { currentSort = SortKey::Date; reverseSort = false; }
          refreshViews();
          statusMessage = "Sort: Date" + getSortArrow(SortKey::Date, reverseSort);
          return true;
        }
        if (event == Event::Character('t')) {
          if (currentSort == SortKey::Title) reverseSort = !reverseSort;
          else { currentSort = SortKey::Title; reverseSort = false; }
          refreshViews();
          statusMessage = "Sort: Title" + getSortArrow(SortKey::Title, reverseSort);
          return true;
        }
        if (event == Event::Character('s')) {
          if (currentSort == SortKey::Status) reverseSort = !reverseSort;
          else { currentSort = SortKey::Status; reverseSort = false; }
          refreshViews();
          statusMessage = "Sort: Status" + getSortArrow(SortKey::Status, reverseSort);
          return true;
        }
        if (event == Event::Character('r')) {
          if (currentSort == SortKey::Score) reverseSort = !reverseSort;
          else { currentSort = SortKey::Score; reverseSort = false; }
          refreshViews();
          statusMessage = "Sort: Rating" + getSortArrow(SortKey::Score, reverseSort);
          return true;
        }
        if (event == Event::Character('p')) {
          if (currentSort == SortKey::Progress) reverseSort = !reverseSort;
          else { currentSort = SortKey::Progress; reverseSort = false; }
          refreshViews();
          statusMessage = "Sort: Progress" + getSortArrow(SortKey::Progress, reverseSort);
          return true;
        }

        // Add / New Entry
        if (event == Event::Character('n') || event == Event::Character('a')) {
          createNewEntry();
          return true;
        }

        // Delete Entry
        if (event == Event::Character('d') && !displayedEntries.empty()) {
          pendingDeleteId = displayedEntries[catalogSelected].id;
          statusMessage = "Delete '" + displayedEntries[catalogSelected].title + "'? Confirm [y/N]";
          return true;
        }

        // QoL Clipboard Copying Binds
        if (event == Event::Character('y') || event == Event::Character('Y')) {
          if (!displayedEntries.empty()) {
            copyToClipboard(displayedEntries[catalogSelected].title);
          }
          return true;
        }
      }

      if (event == Event::Character('/') || event == Event::Special("\x06")) {
        searchInput->TakeFocus();
        currentTier = ActiveTier::Search;
        statusMessage = "Search title... [ret] Apply  [Esc] Clear";
        return true;
      }

      if (event == Event::Character('o') && !displayedEntries.empty()) {
        openInBrowser(displayedEntries[catalogSelected]);
        return true;
      }

      // Tab Switching
      if (event == Event::Tab) {
        if (currentTab == ViewTab::Anime) currentTab = ViewTab::Manga;
        else if (currentTab == ViewTab::Manga) currentTab = ViewTab::Overview;
        else currentTab = ViewTab::Anime;
        catalogSelected = 0;
        refreshViews();
        return true;
      }
      if (event == Event::Character('[') || event == Event::Character(']')) {
        currentTab = (currentTab == ViewTab::Anime) ? ViewTab::Manga : ViewTab::Anime;
        catalogSelected = 0;
        refreshViews();
        return true;
      }
      if (event == Event::Character('1')) { currentTab = ViewTab::Anime; catalogSelected = 0; refreshViews(); return true; }
      if (event == Event::Character('2')) { currentTab = ViewTab::Manga; catalogSelected = 0; refreshViews(); return true; }
      if (event == Event::Character('3')) { currentTab = ViewTab::Overview; return true; }

      // Drill-Down Controls
      if (currentTier == ActiveTier::Sidebar) {
        if (event == Event::Return) {
          catalogMenu->TakeFocus();
          currentTier = ActiveTier::Catalog;
          statusMessage = "Catalog view. [ret] to Edit.";
          return true;
        }
      } else if (currentTier == ActiveTier::Catalog) {
        if (event == Event::Escape || event == Event::Backspace) {
          sidebarMenu->TakeFocus();
          currentTier = ActiveTier::Sidebar;
          statusMessage = "Categories menu. [ret] to Browse.";
          return true;
        }
        if (event == Event::Return && !displayedEntries.empty()) {
          currentTier = ActiveTier::CellEdit;
          activeCellField = EditField::Progress;
          statusMessage = "Editing row. [h/l] Column  [j/k] Value  [ret] Notes  [Esc] Done";
          return true;
        }

        // Navigation Clamping & Smooth Scrolling
        if (event == Event::Character('j') || event == Event::ArrowDown) {
          if (!displayedEntries.empty() && catalogSelected < static_cast<int>(displayedEntries.size()) - 1) {
            catalogSelected++;
            marqueeEntryId = displayedEntries[catalogSelected].id;
            marqueeStartTime = std::chrono::steady_clock::now();
          }
          return true;
        }
        if (event == Event::Character('k') || event == Event::ArrowUp) {
          if (catalogSelected > 0) {
            catalogSelected--;
            marqueeEntryId = displayedEntries[catalogSelected].id;
            marqueeStartTime = std::chrono::steady_clock::now();
          }
          return true;
        }
        if (event == Event::Character('g')) { catalogSelected = 0; return true; }
        if (event == Event::Character('G')) {
          if (!displayedEntries.empty()) catalogSelected = static_cast<int>(displayedEntries.size()) - 1;
          return true;
        }
      }
    }

    return false;
  });

  auto renderOverview = [&]() {
    size_t aWatch = db_.countByStatus(MediaType::Anime, WatchStatus::Watching);
    size_t aComp  = db_.countByStatus(MediaType::Anime, WatchStatus::Completed);
    size_t aHold  = db_.countByStatus(MediaType::Anime, WatchStatus::OnHold);
    size_t aDrop  = db_.countByStatus(MediaType::Anime, WatchStatus::Dropped);
    size_t aPlan  = db_.countByStatus(MediaType::Anime, WatchStatus::PlanToWatch);
    size_t aTotal = db_.getEntries(MediaType::Anime).size();

    size_t mRead  = db_.countByStatus(MediaType::Manga, WatchStatus::Watching);
    size_t mComp  = db_.countByStatus(MediaType::Manga, WatchStatus::Completed);
    size_t mHold  = db_.countByStatus(MediaType::Manga, WatchStatus::OnHold);
    size_t mDrop  = db_.countByStatus(MediaType::Manga, WatchStatus::Dropped);
    size_t mPlan  = db_.countByStatus(MediaType::Manga, WatchStatus::PlanToWatch);
    size_t mTotal = db_.getEntries(MediaType::Manga).size();

    auto makeBar = [](size_t c1, size_t c2, size_t c3, size_t c4, size_t c5, size_t tot) {
      if (tot == 0) return text("Empty dataset.") | dim;
      int width = 74;
      int w1 = std::max(1, static_cast<int>((c1 * width) / tot));
      int w2 = std::max(1, static_cast<int>((c2 * width) / tot));
      int w3 = (c3 > 0) ? std::max(1, static_cast<int>((c3 * width) / tot)) : 0;
      int w4 = (c4 > 0) ? std::max(1, static_cast<int>((c4 * width) / tot)) : 0;
      int w5 = std::max(0, width - (w1 + w2 + w3 + w4));

      return hbox({
        text(std::string(w1, ' ')) | bgcolor(theme::Green),
        text(std::string(w2, ' ')) | bgcolor(theme::Blue),
        (w3 > 0 ? text(std::string(w3, ' ')) | bgcolor(theme::Yellow) : text("")),
        (w4 > 0 ? text(std::string(w4, ' ')) | bgcolor(theme::Red) : text("")),
        (w5 > 0 ? text(std::string(w5, ' ')) | bgcolor(theme::Overlay0) : text(""))
      });
    };

    std::stringstream aDays, mDays;
    aDays << std::fixed << std::setprecision(1) << db_.daysSpent(MediaType::Anime);
    mDays << std::fixed << std::setprecision(1) << db_.daysSpent(MediaType::Manga);

    int aMeanScore = static_cast<int>(std::round(db_.averageScore(MediaType::Anime)));
    int mMeanScore = static_cast<int>(std::round(db_.averageScore(MediaType::Manga)));

    return vbox({
      text(" " + icon::Stats + " Statistics Overview ") | bold | color(theme::Mauve),
      text(""),
      vbox({
        hbox({
          text(" Anime Stats") | bold | color(theme::Lavender),
          filler(),
          text("Days: " + aDays.str() + "  │  Mean: ") | color(theme::Subtext),
          text(renderStarMeter(aMeanScore)) | color(theme::Yellow),
          text(" ")
        }),
        text(""),
        makeBar(aWatch, aComp, aHold, aDrop, aPlan, aTotal),
        text(""),
        hbox({
          vbox({
            hbox({ text(" • Watching      : ") | color(theme::Green), text(std::to_string(aWatch)) | bold | color(theme::Text) }),
            hbox({ text(" • Completed     : ") | color(theme::Blue), text(std::to_string(aComp)) | bold | color(theme::Text) }),
            hbox({ text(" • On-Hold       : ") | color(theme::Yellow), text(std::to_string(aHold)) | bold | color(theme::Text) }),
            hbox({ text(" • Dropped       : ") | color(theme::Red), text(std::to_string(aDrop)) | bold | color(theme::Text) }),
            hbox({ text(" • Plan to Watch : ") | color(theme::Overlay0), text(std::to_string(aPlan)) | bold | color(theme::Text) })
          }) | flex,
          text("  "),
          vbox({
            hbox({ text(" Total Entries   : ") | color(theme::Subtext), text(std::to_string(aTotal)) | bold | color(theme::Text) }),
            hbox({ text(" Rewatched       : ") | color(theme::Subtext), text(std::to_string(db_.totalRewatched(MediaType::Anime))) | bold | color(theme::Text) }),
            hbox({ text(" Episodes Logged : ") | color(theme::Lavender), text(std::to_string(db_.totalProgress(MediaType::Anime))) | bold | color(theme::Text) })
          }) | flex
        })
      }) | border | color(theme::Surface0),
      text(""),
      vbox({
        hbox({
          text(" Manga Stats") | bold | color(theme::Peach),
          filler(),
          text("Days: " + mDays.str() + "  │  Mean: ") | color(theme::Subtext),
          text(renderStarMeter(mMeanScore)) | color(theme::Yellow),
          text(" ")
        }),
        text(""),
        makeBar(mRead, mComp, mHold, mDrop, mPlan, mTotal),
        text(""),
        hbox({
          vbox({
            hbox({ text(" • Reading       : ") | color(theme::Green), text(std::to_string(mRead)) | bold | color(theme::Text) }),
            hbox({ text(" • Completed     : ") | color(theme::Blue), text(std::to_string(mComp)) | bold | color(theme::Text) }),
            hbox({ text(" • On-Hold       : ") | color(theme::Yellow), text(std::to_string(mHold)) | bold | color(theme::Text) }),
            hbox({ text(" • Dropped       : ") | color(theme::Red), text(std::to_string(mDrop)) | bold | color(theme::Text) }),
            hbox({ text(" • Plan to Read  : ") | color(theme::Overlay0), text(std::to_string(mPlan)) | bold | color(theme::Text) })
          }) | flex,
          text("  "),
          vbox({
            hbox({ text(" Total Entries   : ") | color(theme::Subtext), text(std::to_string(mTotal)) | bold | color(theme::Text) }),
            hbox({ text(" Reread          : ") | color(theme::Subtext), text(std::to_string(db_.totalRewatched(MediaType::Manga))) | bold | color(theme::Text) }),
            hbox({ text(" Chapters Logged : ") | color(theme::Lavender), text(std::to_string(db_.totalProgress(MediaType::Manga))) | bold | color(theme::Text) }),
            hbox({ text(" Volumes Logged  : ") | color(theme::Peach), text(std::to_string(db_.totalVolumes(MediaType::Manga))) | bold | color(theme::Text) })
          }) | flex
        })
      }) | border | color(theme::Surface0)
    }) | flex;
  };

  auto mainRenderer = Renderer(globalHandler, [&]() {
    MediaType media = (currentTab == ViewTab::Manga) ? MediaType::Manga : MediaType::Anime;

    size_t aCount = db_.getEntries(MediaType::Anime).size();
    size_t mCount = db_.getEntries(MediaType::Manga).size();

    Element animeTab = (currentTab == ViewTab::Anime)
      ? (text(" " + icon::Anime + " Anime (" + std::to_string(aCount) + ") ") | bold | color(theme::Mauve) | bgcolor(theme::Surface0))
      : (text(" " + icon::Anime + " Anime (" + std::to_string(aCount) + ") ") | color(theme::Subtext));
    animeTab = animeTab | reflect(tabAnimeBox);

    Element mangaTab = (currentTab == ViewTab::Manga)
      ? (text(" " + icon::Manga + " Manga (" + std::to_string(mCount) + ") ") | bold | color(theme::Mauve) | bgcolor(theme::Surface0))
      : (text(" " + icon::Manga + " Manga (" + std::to_string(mCount) + ") ") | color(theme::Subtext));
    mangaTab = mangaTab | reflect(tabMangaBox);

    Element overviewTab = (currentTab == ViewTab::Overview)
      ? (text(" " + icon::Stats + " Overview ") | bold | color(theme::Mauve) | bgcolor(theme::Surface0))
      : (text(" " + icon::Stats + " Overview ") | color(theme::Subtext));
    overviewTab = overviewTab | reflect(tabOverviewBox);

    auto topCard = hbox({
      text(" " + icon::AppLogo + " Catalist ") | bold | color(theme::Mauve),
      text("  "),
      animeTab,
      text(" "),
      mangaTab,
      text(" "),
      overviewTab,
      filler(),
      text("v0.2.0 ") | color(theme::Overlay0)
    }) | border | color(theme::Surface0)
       | size(HEIGHT, EQUAL, 3);

    Element contentPane;

    if (currentTab == ViewTab::Overview) {
      contentPane = renderOverview();
    } else {
      Element details = text("No record selected.") | color(theme::Subtext);
      if (!displayedEntries.empty() && catalogSelected >= 0 && catalogSelected < static_cast<int>(displayedEntries.size())) {
        const auto& e = displayedEntries[catalogSelected];
        float ratio = (e.totalUnits > 0) ? std::clamp(static_cast<float>(e.progress) / e.totalUnits, 0.0f, 1.0f) : 0.0f;
        auto gaugeColor = (ratio >= 1.0f || e.status == WatchStatus::Completed) ? theme::Green : theme::Blue;

        details = vbox({
          (currentTier == ActiveTier::Inspector)
            ? vbox({
                text(" Title:") | bold | color(theme::Mauve),
                inspectorTitleInput->Render() | border | color(theme::Peach),
                text("")
              })
            : vbox({
                text(" " + e.title) | bold | color(theme::Mauve) | reflect(detailsTitleBox),
                text("")
              }),
          hbox({
            text(" " + typeToIcon(e.type) + " " + typeToString(e.type) + " ") | bgcolor(theme::Surface0) | color(typeToColor(e.type)) | bold,
            text(" "),
            text(" " + statusToIcon(e.status) + " " + statusToString(e.status, e.type) + " ") | bgcolor(theme::Surface0) | color(statusToColor(e.status)) | bold | reflect(detailsStatusBox),
            filler(),
            text("[Delete] ") | bold | color(theme::Red) | reflect(detailsDeleteBox),
            text("ID #" + std::to_string(e.id) + " ") | color(theme::Subtext) | reflect(detailsIdBox)
          }),
          text(""),
          hbox({
            text(" Rating: ") | bold | color(theme::Subtext),
            text(renderStarMeter(e.score)) | color(theme::Yellow)
          }) | reflect(detailsRatingBox),
          text(""),
          hbox({
            text(" " + icon::Gauge + " Progress: ") | color(theme::Lavender) | bold,
            text("[-] ") | bold | color(theme::Red) | reflect(detailsProgressMinusBox),
            text(std::to_string(e.progress) + " / " + (e.totalUnits > 0 ? std::to_string(e.totalUnits) : "?") + " ") | color(theme::Text),
            text("[+]") | bold | color(theme::Green) | reflect(detailsProgressPlusBox),
            filler(),
            text(e.totalUnits > 0 ? std::to_string(static_cast<int>(ratio * 100)) + "% " : "Ongoing ") | color(theme::Subtext)
          }),
          text(""),
          gauge(ratio) | color(gaugeColor) | reflect(detailsProgressGaugeBox),
          text(""),
          hbox({ text(" Source MAL ID : ") | color(theme::Subtext), text(e.malId > 0 ? std::to_string(e.malId) : "Local") | color(theme::Text) }) | reflect(detailsMalIdBox),
          hbox({ text(" Start Date    : ") | color(theme::Subtext), text(e.startDate.empty() || e.startDate == "0000-00-00" ? "Not set" : e.startDate) | color(theme::Text) }) | reflect(detailsStartBox),
          hbox({ text(" Finish Date   : ") | color(theme::Subtext), text(e.finishDate.empty() || e.finishDate == "0000-00-00" ? "Not set" : e.finishDate) | color(theme::Text) }) | reflect(detailsFinishBox),
          text(""),
          text(" Notes:") | bold | color(theme::Mauve),
          (currentTier == ActiveTier::Inspector)
            ? (inspectorNotesInput->Render() | border | color(theme::Peach))
            : (text(e.notes.empty() ? " (empty - click or ret to edit)" : " " + e.notes) | color(theme::Subtext) | reflect(detailsNotesBox)),
          filler()
        });
      }

      auto makeHeaderCell = [&](const std::string& name, SortKey key, int width, Box& box, bool isFlex = false) {
        bool active = (currentSort == key);
        std::string label = " " + name;
        if (active) {
          label += getSortArrow(key, reverseSort);
        }
        Element cell = text(label);
        if (isFlex) cell = cell | flex;
        else cell = cell | size(WIDTH, EQUAL, width);

        if (active) {
          cell = cell | bold | color(theme::Mauve) | underlined;
        } else {
          cell = cell | bold | color(theme::Subtext);
        }
        return cell | reflect(box);
      };

      auto headerRibbon = hbox({
        text("  ") | size(WIDTH, EQUAL, 2),
        makeHeaderCell("Date", SortKey::Date, 10, headerDateBox),
        text(" "),
        makeHeaderCell("Title", SortKey::Title, 26, headerTitleBox, true),
        text(" "),
        makeHeaderCell("Status", SortKey::Status, 14, headerStatusBox),
        text(" "),
        makeHeaderCell("Rating", SortKey::Score, 12, headerRatingBox),
        text(" "),
        makeHeaderCell("Progress", SortKey::Progress, 12, headerProgressBox),
        text(" ")
      });

      auto sidebarCard = vbox({
        text("  Categories") | bold | color(theme::Lavender),
        text(""),
        sidebarMenu->Render() | vscroll_indicator | frame | flex
      }) | border | color(currentTier == ActiveTier::Sidebar ? theme::Mauve : theme::Surface0)
         | size(WIDTH, EQUAL, 28)
         | reflect(sidebarCardBox);

      auto searchCard = hbox({
        text(" " + icon::Search + " ") | color(theme::Peach),
        searchInput->Render() | flex,
        text(" "),
        text("[+ New]") | bold | color(theme::Green) | reflect(newEntryBtnBox),
        text(" "),
        text(searchQuery.empty() ? "All entries " : ("Filter: '" + searchQuery + "' ")) | color(theme::Subtext),
        text(" ")
      }) | border | color((currentTier == ActiveTier::Search || searchInput->Focused()) ? theme::Peach : theme::Surface0)
         | size(HEIGHT, EQUAL, 3)
         | reflect(searchBarBox);

      auto catalogCard = vbox({
        headerRibbon,
        text(""),
        displayedEntries.empty()
          ? (text(" No matching entries found.") | color(theme::Subtext) | center | flex)
          : (catalogMenu->Render() | vscroll_indicator | frame | flex)
      }) | border | color((currentTier == ActiveTier::Catalog || currentTier == ActiveTier::CellEdit) ? theme::Mauve : theme::Surface0)
         | flex
         | reflect(catalogCardBox);

      auto detailCard = vbox({
        text("  Details") | bold | color(theme::Lavender),
        text(""),
        details | flex
      }) | border | color(currentTier == ActiveTier::Inspector ? theme::Mauve : theme::Surface0)
         | size(WIDTH, EQUAL, 42)
         | reflect(detailCardBox);

      auto catalogColumn = vbox({
        searchCard,
        catalogCard | flex
      }) | flex;

      auto termSize = Terminal::Size();
      if (termSize.dimx < 110) {
        if (currentTier == ActiveTier::Sidebar) contentPane = sidebarCard | flex;
        else if (currentTier == ActiveTier::Inspector) contentPane = detailCard | flex;
        else contentPane = catalogColumn | flex;
      } else {
        contentPane = hbox({
          sidebarCard,
          catalogColumn,
          detailCard
        });
      }
    }

    Element modalBadge = text(" BROWSE ") | bold | color(theme::Crust) | bgcolor(theme::Mauve);
    if (commandModeActive) {
      modalBadge = text(" COMMAND ") | bold | color(theme::Crust) | bgcolor(theme::Yellow);
    } else if (currentTier == ActiveTier::Search) {
      modalBadge = text(" SEARCH ") | bold | color(theme::Crust) | bgcolor(theme::Peach);
    } else if (currentTier == ActiveTier::Sidebar) {
      modalBadge = text(" CATEGORIES ") | bold | color(theme::Crust) | bgcolor(theme::Lavender);
    } else if (currentTier == ActiveTier::CellEdit) {
      std::string fieldStr = "STATUS";
      if (activeCellField == EditField::Rating) fieldStr = "RATING";
      else if (activeCellField == EditField::Progress) fieldStr = "PROGRESS";
      modalBadge = text(" EDIT " + fieldStr + " ") | bold | color(theme::Crust) | bgcolor(theme::Green);
    } else if (currentTier == ActiveTier::Inspector) {
      modalBadge = text(" NOTES ") | bold | color(theme::Crust) | bgcolor(theme::Peach);
    }

    std::string hint = "c/t/s/r/p Sort  │  n New  │  d Delete  │  ret Edit  │  y Copy  │  / Search  │  q Quit";
    if (commandModeActive) {
      hint = "";
    } else if (currentTier == ActiveTier::Search) {
      hint = "Type query  │  ret/Down Filter  │  Esc Clear & Back";
    } else if (currentTier == ActiveTier::Sidebar) {
      hint = "j/k Move  │  ret Open  │  Esc Back";
    } else if (currentTier == ActiveTier::CellEdit) {
      hint = "h/l Field  │  j/k Change  │  ret Notes  │  Esc Done";
    } else if (currentTier == ActiveTier::Inspector) {
      hint = "Type field  │  ret Save  │  Esc Back";
    }

    auto bottomCard = hbox({
      modalBadge,
      text(" "),
      commandModeActive
        ? (hbox({ text(":" + commandBuffer) | bold | color(theme::Yellow) }))
        : (hbox({ text(hint) | color(theme::Subtext) })),
      filler(),
      text(statusMessage) | color(theme::Yellow),
      text(" ")
    }) | border | color(theme::Surface0)
       | size(HEIGHT, EQUAL, 3);

    return vbox({
      topCard,
      contentPane | flex,
      bottomCard
    });
  });

  std::atomic<bool> appRunning{true};
  std::thread tickerThread([&]() {
    while (appRunning.load()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      screen.PostEvent(Event::Custom);
    }
  });

  screen.Loop(mainRenderer);

  appRunning = false;
  if (tickerThread.joinable()) {
    tickerThread.join();
  }
}

} // namespace catalist
