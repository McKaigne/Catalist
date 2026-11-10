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
inline constexpr const char* kSyncJob = "sync";
// True while a sync runs on the worker thread; the shell keeps the UI off
// the catalogue until it ends.
[[nodiscard]] bool isSyncing(const appContext& ctx);
void startBackgroundSync(appContext& ctx);
void startMalLogin(appContext& ctx);
// Opens AniList's authorize page; false when no client id is configured.
bool openAnilistLogin(appContext& ctx);
// Stores the pasted token and checks it in the background.
void finishAnilistLogin(appContext& ctx, const std::string& token);
void exportMalXml(appContext& ctx);
void restoreBackup(appContext& ctx, const std::string& snapshotName);
void exportCatalog(appContext& ctx);
// Version, catalogue sizes and data directory on one line, for :about.
[[nodiscard]] std::string aboutLine(appContext& ctx);

}  // namespace catalist

#endif  // CATALIST_TUI_ACTIONS_HPP_
