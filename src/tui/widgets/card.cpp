#include "catalist/tui/widgets/card.hpp"

namespace catalist::card {

using namespace ftxui;

Element frame(const std::string& heading, Element body, bool focused,
              const std::string& badge) {
  return vbox({hbox({text("  " + heading) | bold | color(theme::kLavender),
                     filler(), text(badge) | color(theme::kPeach), text(" ")}),
               text(""), std::move(body) | flex}) |
         border | color(focused ? theme::kMauve : theme::kSurface0);
}

Element section(const std::string& label) {
  return text("  " + label) | bold | color(theme::kOverlay0);
}

Element field(bool active, const std::string& label, Element value,
              int labelWidth) {
  return hbox({text(active ? "▌ " : "  ") | color(theme::kMauve) | bold,
               text(label) | size(WIDTH, EQUAL, labelWidth) |
                   (active ? bold : nothing) |
                   color(active ? theme::kPeach : theme::kSubtext),
               std::move(value) | flex});
}

Element choice(bool active, const std::string& label, Color tint) {
  return hbox({text(active ? "▌ " : "  ") | color(theme::kMauve) | bold,
               text(label) | (active ? bold : nothing) |
                   color(active ? theme::kPeach : tint)});
}

Element note(const std::string& line) {
  return text("  " + line) | color(theme::kOverlay0);
}

}  // namespace catalist::card
