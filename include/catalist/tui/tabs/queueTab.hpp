#ifndef CATALIST_TUI_TABS_QUEUE_TAB_HPP_
#define CATALIST_TUI_TABS_QUEUE_TAB_HPP_

#include <vector>

#include "catalist/tui/tab.hpp"

namespace catalist {

// Watching and Plan to Watch entries of both media in the user's own order,
// rearranged with J/K and persisted through entry::queuePos.
class queueTab : public iTab {
public:
  [[nodiscard]] std::string id() const override;
  [[nodiscard]] std::string title() const override;
  [[nodiscard]] std::string icon() const override;

  ftxui::Element render(appContext& ctx) override;
  bool onEvent(const ftxui::Event& event, appContext& ctx) override;
  [[nodiscard]] std::vector<hint> hints() const override;
  void onActivate(appContext& ctx) override;
  [[nodiscard]] std::optional<std::string> countBadge(
      appContext& ctx) const override;

private:
  void refresh(appContext& ctx);
  void moveSelected(appContext& ctx, int direction);
  void stepSelected(appContext& ctx, int delta);
  [[nodiscard]] const entry* selected() const;

  std::vector<entry> queue_;
  int cursor_ = 0;
};

[[nodiscard]] std::vector<entry> queuedEntries(const database& db);

}  // namespace catalist

#endif  // CATALIST_TUI_TABS_QUEUE_TAB_HPP_
