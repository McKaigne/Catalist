#include "catalist/tui/tabs/queueTab.hpp"

#include <algorithm>

#include "catalist/model/entryOps.hpp"
#include "catalist/model/queueOrder.hpp"
#include "catalist/tui/actions.hpp"
#include "catalist/tui/keys.hpp"
#include "catalist/tui/theme/palettes.hpp"
#include "catalist/tui/theme/statusStyle.hpp"
#include "catalist/tui/theme/themeRegistry.hpp"
#include "catalist/tui/widgets/marquee.hpp"

namespace catalist {

using namespace ftxui;

namespace {

std::string progressText(const entry& e) {
  return std::to_string(e.progress) + "/" +
         (e.totalUnits > 0 ? std::to_string(e.totalUnits) : "?");
}

}  // namespace

std::vector<entry> queuedEntries(const database& db) {
  std::vector<entry> all = db.getEntries(mediaType::kAnime);
  const auto& manga = db.getEntries(mediaType::kManga);
  all.insert(all.end(), manga.begin(), manga.end());
  return orderedQueue(all);
}

std::string queueTab::id() const {
  return "queue";
}

std::string queueTab::title() const {
  return "Queue";
}

std::string queueTab::icon() const {
  return icon::kInProgress;
}

void queueTab::onActivate(appContext& ctx) {
  refresh(ctx);
}

std::optional<std::string> queueTab::countBadge(appContext& ctx) const {
  return std::to_string(queuedEntries(ctx.db).size());
}

void queueTab::refresh(appContext& ctx) {
  queue_ = queuedEntries(ctx.db);
  int last = static_cast<int>(queue_.size()) - 1;
  cursor_ = std::clamp(cursor_, 0, std::max(0, last));
}

const entry* queueTab::selected() const {
  bool inRange = cursor_ >= 0 && cursor_ < static_cast<int>(queue_.size());
  return inRange ? &queue_[cursor_] : nullptr;
}

void queueTab::moveSelected(appContext& ctx, int direction) {
  int target = cursor_ + direction;
  if (!selected() || target < 0 || target >= static_cast<int>(queue_.size()))
    return;
  for (const auto& changed : moveInQueue(queue_, static_cast<size_t>(cursor_),
                                         static_cast<size_t>(target)))
    ctx.db.updateEntry(changed);
  ctx.db.saveAll();
  cursor_ = target;
  ctx.notices.setStatus("Queue: moved '" + marquee::clip(queue_[target].title, 30) +
                        "' to #" + std::to_string(target + 1));
}

void queueTab::stepSelected(appContext& ctx, int delta) {
  const entry* current = selected();
  if (!current)
    return;
  entry before = *current;
  entry after = stepProgress(before, delta, localDate(ctx.clock));
  commitEntryChange(ctx, before, after);
  refresh(ctx);
  ctx.notices.setStatus("Progress: " + progressText(after));
}

bool queueTab::onEvent(const Event& event, appContext& ctx) {
  const auto& keybinds = ctx.config.keybinds;
  int last = std::max(0, static_cast<int>(queue_.size()) - 1);
  if (keys::isBound(event, keybinds, keyAction::kMoveItemDown)) {
    moveSelected(ctx, 1);
    return true;
  }
  if (keys::isBound(event, keybinds, keyAction::kMoveItemUp)) {
    moveSelected(ctx, -1);
    return true;
  }
  if (keys::isMoveDown(event, keybinds)) {
    cursor_ = std::min(cursor_ + 1, last);
    return true;
  }
  if (keys::isMoveUp(event, keybinds)) {
    cursor_ = std::max(cursor_ - 1, 0);
    return true;
  }
  if (keys::isJumpTop(event, keybinds)) {
    cursor_ = 0;
    return true;
  }
  if (keys::isJumpBottom(event, keybinds)) {
    cursor_ = last;
    return true;
  }
  if (keys::isBound(event, keybinds, keyAction::kIncrementProgress)) {
    stepSelected(ctx, 1);
    return true;
  }
  if (keys::isBound(event, keybinds, keyAction::kDecrementProgress)) {
    stepSelected(ctx, -1);
    return true;
  }
  if (keys::isBound(event, keybinds, keyAction::kOpenBrowser) && selected()) {
    openInBrowser(ctx, *selected());
    return true;
  }
  return false;
}

Element queueTab::render(appContext&) {
  const auto& theme = themeRegistry::active();
  Elements rows;
  for (size_t i = 0; i < queue_.size(); ++i) {
    const auto& e = queue_[i];
    bool active = static_cast<int>(i) == cursor_;
    float ratio = e.totalUnits > 0
                      ? std::clamp(static_cast<float>(e.progress) / e.totalUnits,
                                   0.0f, 1.0f)
                      : 0.0f;
    auto row = hbox(
        {text(active ? "▌ " : "  ") | color(theme.mauve) | bold,
         text(std::to_string(i + 1) + ".") | size(WIDTH, EQUAL, 5) |
             color(theme.overlay0),
         text(typeToIcon(e.type) + " ") | color(typeToColor(e.type)),
         text(marquee::clip(e.title, 70)) | (active ? bold : nothing) |
             color(theme.text) | flex,
         text(statusToIcon(e.status) + " " + statusToString(e.status, e.type)) |
             size(WIDTH, EQUAL, 16) | color(statusToColor(e.status)),
         text(progressText(e)) | size(WIDTH, EQUAL, 10) | color(theme.lavender),
         gauge(ratio) | size(WIDTH, EQUAL, 20) | color(theme.blue), text(" ")});
    rows.push_back(active ? row | bgcolor(theme.surface0) | focus : row);
  }
  if (rows.empty())
    rows.push_back(text(" Nothing queued: entries you are watching or plan to "
                        "watch appear here.") |
                   color(theme.subtext));
  return vbox({hbox({text("  Watch Queue") | bold | color(theme.lavender),
                     filler(),
                     text(std::to_string(queue_.size()) + " entries ") |
                         color(theme.overlay0)}),
               text(""), vbox(std::move(rows)) | vscroll_indicator | frame | flex}) |
         border | color(theme.surface0) | flex;
}

std::vector<hint> queueTab::hints() const {
  return {{"j/k", "move"}, {"J/K", "reorder"}, {"+/-", "progress"},
          {"o", "open"},   {"q", "quit"}};
}

}  // namespace catalist
