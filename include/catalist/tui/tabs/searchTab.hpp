#ifndef CATALIST_TUI_TABS_SEARCH_TAB_HPP_
#define CATALIST_TUI_TABS_SEARCH_TAB_HPP_

#include <ftxui/component/component.hpp>
#include <string>
#include <vector>

#include "catalist/net/providers/iProvider.hpp"
#include "catalist/tui/tab.hpp"

namespace catalist {

// Full-screen online search across MAL, AniList and Kitsu. Enter adds the
// highlighted result to the library and keeps the results on screen.
class searchTab : public iTab {
public:
  explicit searchTab(const appConfig& config);

  [[nodiscard]] std::string id() const override;
  [[nodiscard]] std::string title() const override;
  [[nodiscard]] std::string icon() const override;

  ftxui::Element render(appContext& ctx) override;
  bool onEvent(const ftxui::Event& event, appContext& ctx) override;
  [[nodiscard]] std::vector<hint> hints() const override;
  void onActivate(appContext& ctx) override;
  ftxui::Component focusRoot() override { return root_; }
  [[nodiscard]] bool capturesInput() const override { return typing_; }

private:
  bool handleTyping(const ftxui::Event& event, appContext& ctx);
  void startTyping();
  void runSearch(appContext& ctx);
  void addSelected(appContext& ctx);
  void cycleProvider(appContext& ctx, int direction);
  [[nodiscard]] std::string providerKey(appContext& ctx) const;
  [[nodiscard]] bool isInLibrary(appContext& ctx,
                                 const providerSearchResult& result) const;

  mediaType media_;
  std::string query_;
  bool typing_ = true;
  int providerIndex_ = 0;
  std::vector<providerSearchResult> results_;
  int cursor_ = 0;
  std::string feedback_;

  ftxui::Component input_;
  ftxui::Component root_;
};

}  // namespace catalist

#endif  // CATALIST_TUI_TABS_SEARCH_TAB_HPP_
