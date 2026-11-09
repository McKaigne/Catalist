#ifndef CATALIST_TUI_KEYS_HPP_
#define CATALIST_TUI_KEYS_HPP_

#include <ftxui/component/event.hpp>

#include "catalist/config/keybindManager.hpp"

// Named predicates for the movement and confirmation keys every tab shares,
// so all tabs honour the same bindings (arrows always work too).
namespace catalist::keys {

[[nodiscard]] bool isBound(const ftxui::Event& event,
                           const keybindManager& keybinds, keyAction action);

[[nodiscard]] bool isMoveUp(const ftxui::Event& event, const keybindManager& keybinds);
[[nodiscard]] bool isMoveDown(const ftxui::Event& event, const keybindManager& keybinds);
[[nodiscard]] bool isMoveLeft(const ftxui::Event& event, const keybindManager& keybinds);
[[nodiscard]] bool isMoveRight(const ftxui::Event& event, const keybindManager& keybinds);
[[nodiscard]] bool isJumpTop(const ftxui::Event& event, const keybindManager& keybinds);
[[nodiscard]] bool isJumpBottom(const ftxui::Event& event, const keybindManager& keybinds);
[[nodiscard]] bool isConfirm(const ftxui::Event& event);
[[nodiscard]] bool isConfirmOrSpace(const ftxui::Event& event);
[[nodiscard]] bool isCharacter(const ftxui::Event& event, char expected);

}  // namespace catalist::keys

#endif  // CATALIST_TUI_KEYS_HPP_
