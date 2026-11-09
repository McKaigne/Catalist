#ifndef CATALIST_TUI_WIDGETS_PILL_HPP_
#define CATALIST_TUI_WIDGETS_PILL_HPP_

#include <ftxui/dom/elements.hpp>
#include <string>

namespace catalist {

// A label with rounded powerline caps in the background colour.
[[nodiscard]] ftxui::Element pill(const std::string& label, ftxui::Color fg,
                                  ftxui::Color bg, bool boldText = true);

}  // namespace catalist

#endif  // CATALIST_TUI_WIDGETS_PILL_HPP_
