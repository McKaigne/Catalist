#ifndef CATALIST_TUI_KEYS_HPP_
#define CATALIST_TUI_KEYS_HPP_

#include <ftxui/component/event.hpp>

// The fixed key map, plus named predicates for the movement and confirmation
// keys every tab shares (arrows always work too).
namespace catalist {

enum class keyAction {
  kMoveUp,
  kMoveDown,
  kJumpTop,
  kJumpBottom,
  kFocusLeft,
  kFocusRight,
  kIncrementProgress,
  kDecrementProgress,
  kEditNotes,
  kDeleteEntry,
  kSortDate,
  kSortTitle,
  kSortStatus,
  kSortRating,
  kSortProgress,
  kSearchFilter,
  kOpenBrowser,
  kYankTitle,
  kUndo,
  kRedo,
  kCommandPalette,
  kQuit,
  kToggleMedia
};

namespace keys {

[[nodiscard]] bool isBound(const ftxui::Event& event, keyAction action);

[[nodiscard]] bool isMoveUp(const ftxui::Event& event);
[[nodiscard]] bool isMoveDown(const ftxui::Event& event);
[[nodiscard]] bool isMoveLeft(const ftxui::Event& event);
[[nodiscard]] bool isMoveRight(const ftxui::Event& event);
[[nodiscard]] bool isJumpTop(const ftxui::Event& event);
[[nodiscard]] bool isJumpBottom(const ftxui::Event& event);
[[nodiscard]] bool isConfirm(const ftxui::Event& event);
[[nodiscard]] bool isConfirmOrSpace(const ftxui::Event& event);
[[nodiscard]] bool isCharacter(const ftxui::Event& event, char expected);

}  // namespace keys
}  // namespace catalist

#endif  // CATALIST_TUI_KEYS_HPP_
