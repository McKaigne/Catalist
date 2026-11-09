#ifndef CATALIST_TUI_ACTIONS_HPP_
#define CATALIST_TUI_ACTIONS_HPP_

#include <optional>
#include <string>

#include "catalist/tui/appContext.hpp"

// Side-effecting user actions shared by several tabs and the command palette.
namespace catalist {

void openInBrowser(appContext& ctx, const entry& target);
void copyToClipboard(appContext& ctx, const std::string& text);
[[nodiscard]] std::optional<std::string> pickXmlFile(appContext& ctx);
void importXmlInteractively(appContext& ctx);
void createBackup(appContext& ctx);
void startBackgroundSync(appContext& ctx);

}  // namespace catalist

#endif  // CATALIST_TUI_ACTIONS_HPP_
