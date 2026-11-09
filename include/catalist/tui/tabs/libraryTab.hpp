#ifndef CATALIST_TUI_TABS_LIBRARY_TAB_HPP_
#define CATALIST_TUI_TABS_LIBRARY_TAB_HPP_

#include <optional>
#include <vector>

#include "catalist/tui/tab.hpp"

namespace catalist {

// Three-column browser: start year, then status within that year, then the
// matching entries.
class libraryTab : public iTab {
public:
  [[nodiscard]] std::string id() const override;
  [[nodiscard]] std::string title() const override;
  [[nodiscard]] std::string icon() const override;

  ftxui::Element render(appContext& ctx) override;
  bool onEvent(const ftxui::Event& event, appContext& ctx) override;
  [[nodiscard]] std::vector<hint> hints() const override;
  void onActivate(appContext& ctx) override;

private:
  enum class column { kYears, kStatuses, kEntries };

  void refresh(appContext& ctx);
  [[nodiscard]] int& cursorFor(column which);
  [[nodiscard]] int lastIndexFor(column which) const;
  [[nodiscard]] std::vector<entry> entriesFor(const database& db,
                                              std::optional<int> year,
                                              std::optional<watchStatus> status) const;

  mediaType media_ = mediaType::kAnime;
  column focus_ = column::kYears;
  // Absent year means "no start date".
  std::vector<std::optional<int>> years_;
  std::vector<watchStatus> statuses_;
  std::vector<entry> entries_;
  int yearCursor_ = 0;
  int statusCursor_ = 0;
  int entryCursor_ = 0;
};

}  // namespace catalist

#endif  // CATALIST_TUI_TABS_LIBRARY_TAB_HPP_
