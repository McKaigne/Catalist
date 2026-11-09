#include "catalist/tui/widgets/marquee.hpp"

#include <chrono>

namespace catalist::marquee {

namespace {

size_t utf8SequenceLength(unsigned char lead) {
  if ((lead & 0x80) == 0)
    return 1;
  if ((lead & 0xE0) == 0xC0)
    return 2;
  if ((lead & 0xF0) == 0xE0)
    return 3;
  if ((lead & 0xF8) == 0xF0)
    return 4;
  return 1;
}

uint64_t steadyNowMs() {
  return static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count());
}

}  // namespace

std::vector<std::string> splitGlyphs(const std::string& text) {
  std::vector<std::string> glyphs;
  size_t position = 0;
  while (position < text.size()) {
    size_t length = utf8SequenceLength(static_cast<unsigned char>(text[position]));
    length = std::min(length, text.size() - position);
    glyphs.push_back(text.substr(position, length));
    position += length;
  }
  return glyphs;
}

std::string clip(const std::string& text, size_t maxGlyphs) {
  auto glyphs = splitGlyphs(text);
  if (glyphs.size() <= maxGlyphs)
    return text;
  std::string clipped;
  for (size_t i = 0; i < maxGlyphs; ++i)
    clipped += glyphs[i];
  return clipped;
}

std::string scroll(const std::string& text, size_t maxGlyphs,
                   uint64_t elapsedMs, uint64_t pauseMs, uint64_t stepMs) {
  if (splitGlyphs(text).size() <= maxGlyphs)
    return text;

  auto loop = splitGlyphs(text + "   •   ");
  uint64_t cycleMs = pauseMs + loop.size() * stepMs;
  uint64_t cycleTime = elapsedMs % cycleMs;
  size_t offset = cycleTime < pauseMs
                      ? 0
                      : static_cast<size_t>((cycleTime - pauseMs) / stepMs) %
                            loop.size();

  std::string window;
  for (size_t i = 0; i < maxGlyphs; ++i)
    window += loop[(offset + i) % loop.size()];
  return window;
}

uint64_t selectionTimer::elapsedFor(int selectedIndex) {
  uint64_t now = steadyNowMs();
  if (trackedIndex_ != selectedIndex) {
    trackedIndex_ = selectedIndex;
    changedAtMs_ = now;
  }
  return now >= changedAtMs_ ? now - changedAtMs_ : 0;
}

}  // namespace catalist::marquee
