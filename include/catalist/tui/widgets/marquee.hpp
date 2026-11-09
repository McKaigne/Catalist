#ifndef CATALIST_TUI_WIDGETS_MARQUEE_HPP_
#define CATALIST_TUI_WIDGETS_MARQUEE_HPP_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace catalist::marquee {

// Splits UTF-8 text into code points so clipping never cuts a glyph in half.
[[nodiscard]] std::vector<std::string> splitGlyphs(const std::string& text);

[[nodiscard]] std::string clip(const std::string& text, size_t maxGlyphs);

// Shows the first maxGlyphs for pauseMs, then scrolls one glyph per stepMs
// and loops. Text that fits is returned unchanged.
[[nodiscard]] std::string scroll(const std::string& text, size_t maxGlyphs,
                                 uint64_t elapsedMs, uint64_t pauseMs = 2000,
                                 uint64_t stepMs = 250);

// Restarts the scroll whenever the highlighted row changes.
class selectionTimer {
public:
  [[nodiscard]] uint64_t elapsedFor(int selectedIndex);

private:
  std::optional<int> trackedIndex_;
  uint64_t changedAtMs_ = 0;
};

}  // namespace catalist::marquee

#endif  // CATALIST_TUI_WIDGETS_MARQUEE_HPP_
