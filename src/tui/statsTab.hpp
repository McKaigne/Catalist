#ifndef CATALIST_TUI_STATSTAB_HPP_
#define CATALIST_TUI_STATSTAB_HPP_

#include "tui/app.hpp"

namespace catalist {

// Totals, the status breakdown and the score histogram, for one media type
// at a time.
class statsTab : public iTab {
public:
  [[nodiscard]] std::string title() const override;
  [[nodiscard]] std::string icon() const override;

  ftxui::Element render(appContext& ctx) override;
  bool onEvent(const ftxui::Event& event, appContext& ctx) override;
  [[nodiscard]] std::vector<hint> hints() const override;

private:
  mediaType media_ = mediaType::kAnime;
};

}  // namespace catalist

#endif  // CATALIST_TUI_STATSTAB_HPP_
