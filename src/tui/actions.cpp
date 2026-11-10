#include "catalist/tui/actions.hpp"

#include <vector>

#include "catalist/model/activity.hpp"
#include "catalist/model/serviceLinks.hpp"
#include "catalist/tui/widgets/marquee.hpp"

namespace catalist {

void commitEntryChange(appContext& ctx, const entry& before,
                       const entry& after) {
  ctx.undo.record({historyAction::kMutate, after.type, before, after});
  ctx.db.updateEntry(after);
  ctx.db.logActivity({activityTimestamp(ctx.clock), after.type, after.title,
                      after.status, after.progress, after.totalUnits,
                      after.score});
  ctx.db.saveAll();
}

void openInBrowser(appContext& ctx, const entry& target) {
  std::string url = webAddress(target);
  if (url.empty()) {
    ctx.notices.push(
        "Error: No external provider links associated with this entry.");
    return;
  }
  ctx.launcher.spawnDetached(browserOpenCommand(url));
  ctx.notices.setStatus("Opened: " + url);
}

void copyToClipboard(appContext& ctx, const std::string& text) {
  if (text.empty())
    return;
  const std::vector<std::vector<std::string>> clipboardTools = {
      {"wl-copy"}, {"xclip", "-selection", "clipboard"}, {"pbcopy"}};
  for (const auto& tool : clipboardTools) {
    if (ctx.launcher.feed(tool, text))
      break;
  }
  ctx.notices.push("Copied: \"" + marquee::clip(text, 42) + "\"");
}

}  // namespace catalist
