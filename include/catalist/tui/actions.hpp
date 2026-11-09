#ifndef CATALIST_TUI_ACTIONS_HPP_
#define CATALIST_TUI_ACTIONS_HPP_

#include <optional>
#include <string>

#include "catalist/tui/appContext.hpp"

// Side-effecting user actions shared by several tabs and the command palette.
namespace catalist {

// Applies one edit the way every tab must: undo record, database update,
// history entry and save.
void commitEntryChange(appContext& ctx, const entry& before,
                       const entry& after);
void openInBrowser(appContext& ctx, const entry& target);
void copyToClipboard(appContext& ctx, const std::string& text);
[[nodiscard]] std::optional<std::string> pickXmlFile(appContext& ctx);
void importXmlInteractively(appContext& ctx);
void createBackup(appContext& ctx);
void startBackgroundSync(appContext& ctx);
void startMalLogin(appContext& ctx);
void restoreBackup(appContext& ctx, const std::string& snapshotName);
void exportCatalog(appContext& ctx);

}  // namespace catalist

#endif  // CATALIST_TUI_ACTIONS_HPP_
