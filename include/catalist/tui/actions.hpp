#ifndef CATALIST_TUI_ACTIONS_HPP_
#define CATALIST_TUI_ACTIONS_HPP_

#include <string>

#include "catalist/tui/appContext.hpp"

// Side-effecting user actions of the catalogue tabs.
namespace catalist {

// Applies one edit the way every tab must: undo record, database update,
// history entry and save.
void commitEntryChange(appContext& ctx, const entry& before,
                       const entry& after);
void openInBrowser(appContext& ctx, const entry& target);
void copyToClipboard(appContext& ctx, const std::string& text);

}  // namespace catalist

#endif  // CATALIST_TUI_ACTIONS_HPP_
