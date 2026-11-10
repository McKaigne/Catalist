#ifndef CATALIST_TUI_WIDGETS_CARD_HPP_
#define CATALIST_TUI_WIDGETS_CARD_HPP_

#include <ftxui/dom/elements.hpp>
#include <string>

#include "catalist/tui/theme/palettes.hpp"

// The building blocks of the catalogue's look, shared by every tab.
namespace catalist::card {

// A rounded card: bold lavender heading, an optional badge on the right, a
// blank line and the body. The border turns mauve while the card has focus.
[[nodiscard]] ftxui::Element frame(const std::string& heading,
                                   ftxui::Element body, bool focused,
                                   const std::string& badge = "");

// An upper-case sub-heading inside a card ("STATUS", "METRICS").
[[nodiscard]] ftxui::Element section(const std::string& label);

// A labelled row; `active` draws the cursor bar and highlights the label.
[[nodiscard]] ftxui::Element field(bool active, const std::string& label,
                                   ftxui::Element value, int labelWidth = 14);

// A selectable line in a list card, with the same cursor bar.
[[nodiscard]] ftxui::Element choice(bool active, const std::string& label,
                                    ftxui::Color tint);

// A dim explanatory line.
[[nodiscard]] ftxui::Element note(const std::string& text);

}  // namespace catalist::card

#endif  // CATALIST_TUI_WIDGETS_CARD_HPP_
