#include "catalist/tui/tabs/searchTab.hpp"

#include <algorithm>
#include <cstdio>

#include "catalist/tui/keys.hpp"
#include "catalist/tui/theme/palettes.hpp"
#include "catalist/tui/theme/themeRegistry.hpp"
#include "catalist/tui/widgets/marquee.hpp"
#include "catalist/tui/widgets/pill.hpp"

namespace catalist {

using namespace ftxui;

namespace {

constexpr size_t kResultLimit = 15;

entry entryFromResult(const providerSearchResult& result, const isoDate& today) {
  entry added;
  added.type = result.type;
  added.title = result.title;
  added.status = watchStatus::kWatching;
  added.totalUnits = result.totalUnits;
  added.setRemoteId(result.providerKey, result.remoteId);
  if (result.providerKey == "mal")
    added.id = added.malId;
  added.startDate = isoDate::parse(result.year + "-01-01").value_or(today);
  added.updatedAt = today;
  return added;
}

std::string formatScore(double score) {
  if (score <= 0.0)
    return " -  ";
  char buffer[16];
  std::snprintf(buffer, sizeof(buffer), "%.2f", score);
  return buffer;
}

}  // namespace

searchTab::searchTab(const appConfig& config)
    : media_(config.defaultMedia == "manga" ? mediaType::kManga
                                            : mediaType::kAnime) {
  InputOption option;
  option.transform = [](InputState state) {
    const auto& theme = themeRegistry::active();
    Element field = state.element | color(state.is_placeholder ? theme.overlay0
                                                               : theme.text);
    return state.focused ? field | color(theme.peach) : field;
  };
  input_ = Input(&query_, "Type a title and press [ret]...", option);
  root_ = Container::Vertical({input_});
}

std::string searchTab::id() const {
  return "search";
}

std::string searchTab::title() const {
  return "Search";
}

std::string searchTab::icon() const {
  return icon::kSearch;
}

void searchTab::onActivate(appContext&) {
  if (results_.empty())
    startTyping();
}

std::string searchTab::providerKey(appContext& ctx) const {
  auto providers = ctx.providers.getAllProviders();
  if (providers.empty())
    return "";
  return providers[providerIndex_ % providers.size()]->getKey();
}

void searchTab::startTyping() {
  typing_ = true;
  input_->TakeFocus();
}

void searchTab::cycleProvider(appContext& ctx, int direction) {
  int count = static_cast<int>(ctx.providers.getAllProviders().size());
  if (count == 0)
    return;
  providerIndex_ = (providerIndex_ + direction + count) % count;
  results_.clear();
  feedback_ = "Provider: " + providerKey(ctx) + ". Press [/] to search.";
}

void searchTab::runSearch(appContext& ctx) {
  typing_ = false;
  auto provider = ctx.providers.getProvider(providerKey(ctx));
  if (query_.empty() || !provider)
    return;
  results_ = provider->search(media_, query_, kResultLimit);
  cursor_ = 0;
  feedback_ = results_.empty()
                  ? "No results from " + provider->getDisplayName() + "."
                  : std::to_string(results_.size()) + " results from " +
                        provider->getDisplayName() + ".";
}

bool searchTab::isInLibrary(appContext& ctx,
                            const providerSearchResult& result) const {
  for (const auto& owned : ctx.db.getEntries(result.type)) {
    if (owned.getRemoteId(result.providerKey) == result.remoteId)
      return true;
  }
  return false;
}

void searchTab::addSelected(appContext& ctx) {
  if (cursor_ < 0 || cursor_ >= static_cast<int>(results_.size()))
    return;
  const auto& picked = results_[cursor_];
  entry added = entryFromResult(picked, localDate(ctx.clock));
  int assignedId = ctx.db.addEntry(added);
  added.id = assignedId;
  ctx.undo.record({historyAction::kAdd, added.type, entry{}, added});
  ctx.db.saveAll();
  ctx.notices.setStatus("Added '" + marquee::clip(picked.title, 40) + "' to " +
                        typeToString(picked.type) + ".");
}

bool searchTab::handleTyping(const Event& event, appContext& ctx) {
  if (event == Event::Escape) {
    typing_ = false;
    return true;
  }
  if (keys::isConfirm(event)) {
    runSearch(ctx);
    return true;
  }
  input_->OnEvent(event);
  return true;
}

bool searchTab::onEvent(const Event& event, appContext& ctx) {
  if (typing_)
    return handleTyping(event, ctx);
  const auto& keybinds = ctx.config.keybinds;
  int last = std::max(0, static_cast<int>(results_.size()) - 1);
  if (keys::isBound(event, keybinds, keyAction::kSearchFilter) ||
      keys::isCharacter(event, 'i')) {
    startTyping();
    return true;
  }
  if (keys::isBound(event, keybinds, keyAction::kNextProvider) ||
      keys::isMoveRight(event, keybinds)) {
    cycleProvider(ctx, 1);
    return true;
  }
  if (keys::isMoveLeft(event, keybinds)) {
    cycleProvider(ctx, -1);
    return true;
  }
  if (keys::isBound(event, keybinds, keyAction::kToggleMedia)) {
    media_ = media_ == mediaType::kAnime ? mediaType::kManga : mediaType::kAnime;
    results_.clear();
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
  if (!keys::isConfirm(event))
    return false;
  addSelected(ctx);
  return true;
}

Element searchTab::render(appContext& ctx) {
  const auto& theme = themeRegistry::active();
  std::string activeKey = providerKey(ctx);

  Elements switcher = {text(" Provider: ") | color(theme.subtext)};
  for (const auto& provider : ctx.providers.getAllProviders()) {
    bool active = provider->getKey() == activeKey;
    switcher.push_back(active ? pill(provider->getDisplayName(), theme.crust,
                                     theme.mauve)
                              : text(" " + provider->getDisplayName() + " ") |
                                    color(theme.subtext));
    switcher.push_back(text(" "));
  }
  bool isAnime = media_ == mediaType::kAnime;
  switcher.push_back(filler());
  switcher.push_back(text(isAnime ? " Anime " : " Manga ") | bold |
                     color(isAnime ? theme.blue : theme.peach));

  Elements rows;
  for (size_t i = 0; i < results_.size(); ++i) {
    const auto& result = results_[i];
    bool active = static_cast<int>(i) == cursor_;
    auto badge = ctx.providers.getProvider(result.providerKey);
    std::string units = result.totalUnits > 0
                            ? std::to_string(result.totalUnits) +
                                  (isAnime ? " eps" : " chs")
                            : "ongoing";
    auto row = hbox(
        {text(active ? "▌ " : "  ") | color(theme.mauve) | bold,
         pill(badge ? badge->getShortBadge() : result.providerKey, theme.crust,
              theme.lavender, false),
         text(" "),
         text(marquee::clip(result.title, 60)) | (active ? bold : nothing) |
             color(theme.text) | flex,
         text(result.year.empty() ? "  -  " : result.year) |
             size(WIDTH, EQUAL, 6) | color(theme.subtext),
         text(units) | size(WIDTH, EQUAL, 10) | color(theme.subtext),
         text("★ " + formatScore(result.score)) | size(WIDTH, EQUAL, 8) |
             color(theme.yellow),
         text(isInLibrary(ctx, result) ? " ✓ in library " : "              ") |
             color(theme.green)});
    rows.push_back(active ? row | bgcolor(theme.surface0) | focus : row);
  }
  if (rows.empty())
    rows.push_back(text(feedback_.empty()
                            ? " Type a title and press [ret] to search."
                            : " " + feedback_) |
                   color(theme.subtext) | center);

  auto inputCard = hbox({text(" " + icon::kSearch + " ") | color(theme.peach),
                         input_->Render() | flex}) |
                   border | color(typing_ ? theme.peach : theme.surface0);
  return vbox({hbox(std::move(switcher)), inputCard,
               vbox(std::move(rows)) | vscroll_indicator | frame | flex | border |
                   color(typing_ ? theme.surface0 : theme.mauve),
               text(" " + feedback_) | color(theme.overlay0)}) |
         flex;
}

std::vector<hint> searchTab::hints() const {
  if (typing_)
    return {{"ret", "search"}, {"Esc", "results"}};
  return {{"/", "type"},       {"h/l p", "provider"}, {"m", "anime/manga"},
          {"j/k", "move"},     {"ret", "add"},        {"q", "quit"}};
}

}  // namespace catalist
