#ifndef CATALIST_TUI_KEYS_HPP_
#define CATALIST_TUI_KEYS_HPP_

#include <ftxui/component/event.hpp>

// Named predicates for the keys that have more than one spelling: movement
// (arrows always work too), confirmation and the progress step.
namespace catalist {

namespace keys {

[[nodiscard]] bool isMoveUp(const ftxui::Event& event);
[[nodiscard]] bool isMoveDown(const ftxui::Event& event);
[[nodiscard]] bool isMoveLeft(const ftxui::Event& event);
[[nodiscard]] bool isMoveRight(const ftxui::Event& event);
[[nodiscard]] bool isJumpTop(const ftxui::Event& event);
[[nodiscard]] bool isJumpBottom(const ftxui::Event& event);
[[nodiscard]] bool isConfirm(const ftxui::Event& event);
[[nodiscard]] bool isConfirmOrSpace(const ftxui::Event& event);
// '+', or the unshifted '=' that shares its key on most layouts.
[[nodiscard]] bool isIncrement(const ftxui::Event& event);
[[nodiscard]] bool isCharacter(const ftxui::Event& event, char expected);

}  // namespace keys
}  // namespace catalist

#endif  // CATALIST_TUI_KEYS_HPP_
