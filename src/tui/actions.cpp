#include "catalist/tui/actions.hpp"

#include <vector>

#include "catalist/model/activity.hpp"
#include "catalist/net/metadataSync.hpp"
#include "catalist/storage/backup.hpp"
#include "catalist/storage/malParser.hpp"
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
  bool started = ctx.jobs.start("sync", [&ctx]() {
    std::string outcome;
    metadataSync::syncOngoing(ctx.db, ctx.http, outcome);
    return outcome;
  });
  if (!started) {
    ctx.notices.push("Another background task is still running...");
    return;
  }
  ctx.notices.setStatus("Reconciling ongoing titles with MyAnimeList...");
}

void startMalLogin(appContext& ctx) {
  bool started = ctx.jobs.start("login", [&ctx]() {
    std::string outcome;
    ctx.auth.authenticate(outcome);
    return outcome;
  });
  if (!started) {
    ctx.notices.push("Another background task is still running...");
    return;
  }
  ctx.notices.setStatus("Waiting for MyAnimeList login in your browser...");
}

// The chosen snapshot is read before the safety snapshot is taken: both use
// second-resolution names, so the safety copy may overwrite the very file
// being restored.
void restoreBackup(appContext& ctx, const std::string& snapshotName) {
  auto dataDir = ctx.db.getConfigPath();
  auto snapshot = backup::readSnapshot(ctx.fileSystem, dataDir, snapshotName);
  if (!snapshot) {
    ctx.notices.push("Could not restore " + snapshotName + ".");
    return;
  }
  ctx.db.createBackupSnapshot();
  if (!backup::writeSnapshot(ctx.fileSystem, dataDir, *snapshot)) {
    ctx.notices.push("Could not restore " + snapshotName + ".");
    return;
  }
  ctx.db.loadAll();
  ctx.undo.clear();
  ctx.notices.push("Restored " + snapshotName +
                   " (the previous data was backed up first).");
}

void exportCatalog(appContext& ctx) {
  auto written = backup::exportCatalog(ctx.fileSystem, ctx.clock, ctx.db);
  if (written.empty()) {
    ctx.notices.push("Export failed.");
    return;
  }
  ctx.notices.push("Exported " + std::to_string(written.size()) +
                   " CSV files to " + written.front().parent_path().string());
}

}  // namespace catalist
