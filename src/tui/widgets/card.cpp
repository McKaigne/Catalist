#include "catalist/tui/widgets/card.hpp"

namespace catalist::card {

using namespace ftxui;

Element frame(const std::string& heading, Element body, bool focused,
              const std::string& badge) {
  const auto& theme = themeRegistry::active();
  return vbox({hbox({text("  " + heading) | bold | color(theme.lavender),
                     filler(), text(badge) | color(theme.peach), text(" ")}),
               text(""), std::move(body) | flex}) |
         border | color(focused ? theme.mauve : theme.surface0);
}

Element section(const std::string& label) {
  const auto& theme = themeRegistry::active();
  return text("  " + label) | bold | color(theme.overlay0);
}

Element field(bool active, const std::string& label, Element value,
              int labelWidth) {
  const auto& theme = themeRegistry::active();
  return hbox({text(active ? "▌ " : "  ") | color(theme.mauve) | bold,
               text(label) | size(WIDTH, EQUAL, labelWidth) |
                   (active ? bold : nothing) |
                   color(active ? theme.peach : theme.subtext),
               std::move(value) | flex});
}

Element choice(bool active, const std::string& label, Color tint) {
  const auto& theme = themeRegistry::active();
  return hbox({text(active ? "▌ " : "  ") | color(theme.mauve) | bold,
               text(label) | (active ? bold : nothing) |
                   color(active ? theme.peach : tint)});
}

Element note(const std::string& line) {
  return text("  " + line) | color(themeRegistry::active().overlay0);
}

}  // namespace catalist::card
