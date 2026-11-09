#include "catalist/tui/actions.hpp"

#include <vector>

#include "catalist/storage/malParser.hpp"
#include "catalist/tui/widgets/marquee.hpp"

namespace catalist {

void openInBrowser(appContext& ctx, const entry& target) {
  std::string url = ctx.providers.resolveUrl(target);
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

std::optional<std::string> pickXmlFile(appContext& ctx) {
  auto picked = ctx.launcher.capture({"zenity", "--file-selection",
                                      "--title=Select MyAnimeList XML Export",
                                      "--file-filter=*.xml"});
  if (!picked.ok())
    return std::nullopt;
  std::string chosen = picked.value().standardOutput;
  if (!chosen.empty() && chosen.back() == '\n')
    chosen.pop_back();
  if (chosen.empty())
    return std::nullopt;
  return chosen;
}

void importXmlInteractively(appContext& ctx) {
  auto chosen = pickXmlFile(ctx);
  if (!chosen)
    return;
  size_t count = malParser::importXml(ctx.fileSystem, *chosen, ctx.db, false);
  ctx.notices.push("Ingested " + std::to_string(count) +
                   " entries from XML backup.");
}

void createBackup(appContext& ctx) {
  ctx.db.createBackupSnapshot();
  ctx.notices.push("Created snapshot backup in ~/.config/catalist/backups/.");
}

void startBackgroundSync(appContext& ctx) {
  if (!ctx.sync.start()) {
    ctx.notices.push("Sync already active in background...");
    return;
  }
  ctx.notices.setStatus("Reconciling ongoing titles with MyAnimeList...");
}

}  // namespace catalist
