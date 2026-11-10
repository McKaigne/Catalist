#ifndef CATALIST_TUI_TABS_SETTINGS_TAB_HPP_
#define CATALIST_TUI_TABS_SETTINGS_TAB_HPP_

#include <optional>
#include <string>
#include <vector>

#include "catalist/tui/tab.hpp"

namespace catalist {

// Four cards: themes (previewed live while the cursor moves), preferences,
// the MyAnimeList and AniList accounts with two-way sync, and data (import,
// export, backups).
class settingsTab : public iTab {
public:
  explicit settingsTab(std::vector<std::string> tabIds);

  [[nodiscard]] std::string id() const override;
  [[nodiscard]] std::string title() const override;
  [[nodiscard]] std::string icon() const override;

  ftxui::Element render(appContext& ctx) override;
  bool onEvent(const ftxui::Event& event, appContext& ctx) override;
  [[nodiscard]] std::vector<hint> hints() const override;
  void onActivate(appContext& ctx) override;
  void onDeactivate(appContext& ctx) override;
  [[nodiscard]] bool capturesInput() const override {
    return pendingRestore_.has_value() || pastedToken_.has_value();
  }

private:
  enum class panel { kThemes, kPreferences, kAccounts, kData };

  bool handlePendingRestore(const ftxui::Event& event, appContext& ctx);
  bool handleTokenPaste(const ftxui::Event& event, appContext& ctx);
  void previewThemeUnderCursor();
  void revertPreview(appContext& ctx);
  void applySelectedTheme(appContext& ctx);
  void cyclePreference(appContext& ctx);
  void toggleMalAccount(appContext& ctx);
  void toggleAnilistAccount(appContext& ctx);
  void runAccountAction(appContext& ctx);
  void runDataAction(appContext& ctx);
  void activateRow(appContext& ctx);
  void moveCursor(int delta, appContext& ctx);
  [[nodiscard]] int& cursorOfActivePanel();
  [[nodiscard]] int lastRow(appContext& ctx) const;

  ftxui::Element renderThemes(appContext& ctx) const;
  ftxui::Element renderPreferences(appContext& ctx) const;
  ftxui::Element renderAccounts(appContext& ctx) const;
  ftxui::Element renderTokenPrompt() const;
  ftxui::Element renderData(appContext& ctx) const;

  std::vector<std::string> tabIds_;
  panel activePanel_ = panel::kThemes;
  int themeCursor_ = 0;
  int preferenceCursor_ = 0;
  int accountCursor_ = 0;
  int dataCursor_ = 0;
  std::optional<std::string> pendingRestore_;
  // The AniList token being pasted, while the paste prompt is open.
  std::optional<std::string> pastedToken_;
};

}  // namespace catalist

#endif  // CATALIST_TUI_TABS_SETTINGS_TAB_HPP_
