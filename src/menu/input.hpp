#ifndef CATALIST_MENU_INPUT_HPP_
#define CATALIST_MENU_INPUT_HPP_

#include <optional>
#include <string>

#include "menu/app.hpp"

namespace catalist {

// Nothing when the input has ended.
[[nodiscard]] std::optional<std::string> ReadLine(App& app, const std::string& prompt);
// Nothing on an empty answer too, which is how a prompt is left.
[[nodiscard]] std::optional<std::string> AskText(App& app, const std::string& prompt);
[[nodiscard]] bool AskYes(App& app, const std::string& question);
[[nodiscard]] std::optional<int> ParseNumber(const std::string& text);
// Asks until the answer is a number from 0 to `highest`. The end of the input
// counts as 0, so every menu can be left.
[[nodiscard]] int ReadChoice(App& app, int highest);
void Pause(App& app);

}  // namespace catalist

#endif  // CATALIST_MENU_INPUT_HPP_
