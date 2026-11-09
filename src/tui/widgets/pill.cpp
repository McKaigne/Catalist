#include "catalist/tui/widgets/pill.hpp"

namespace catalist {

using namespace ftxui;

Element pill(const std::string& label, Color fg, Color bg, bool boldText) {
  static const std::string kCapLeft = "\xEE\x82\xB6";
  static const std::string kCapRight = "\xEE\x82\xB4";
  auto content = text(label) | color(fg) | bgcolor(bg);
  if (boldText)
    content = content | bold;
  return hbox({text(kCapLeft) | color(bg), content, text(kCapRight) | color(bg)});
}

}  // namespace catalist
