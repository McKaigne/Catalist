#include "catalist/tui/actions.hpp"

#include <vector>

#include "catalist/core/version.hpp"
#include "catalist/model/activity.hpp"
#include "catalist/model/serviceLinks.hpp"
#include "catalist/net/listSync.hpp"
#include "catalist/storage/backup.hpp"
#include "catalist/storage/malXml.hpp"
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
  auto titles = malXml::parse(ctx.fileSystem.readFile(*chosen).value_or(""));
  for (const auto& title : titles)
    ctx.db.addEntry(title);
  ctx.db.saveAll();
  size_t count = titles.size();
  ctx.notices.push("Ingested " + std::to_string(count) +
                   " entries from XML backup.");
}

void createBackup(appContext& ctx) {
  ctx.db.createBackupSnapshot();
  ctx.notices.push("Created snapshot backup in ~/.config/catalist/backups/.");
}

bool isSyncing(const appContext& ctx) {
  return ctx.jobs.runningName() == kSyncJob;
}

void startBackgroundSync(appContext& ctx) {
  bool connected = ctx.accounts.malLogin.isAuthenticated() ||
                   ctx.accounts.anilistLogin.isAuthenticated();
  if (!connected) {
    ctx.notices.push("Log in to MyAnimeList or AniList in Settings → Accounts & Sync first.");
    return;
  }
  bool started = ctx.jobs.start(kSyncJob, [&ctx]() {
    listSync sync(ctx.db, ctx.clock, ctx.accounts.services(),
                  &ctx.accounts.anilist);
    return "Sync: " + sync.run().summary();
  });
  if (!started) {
    ctx.notices.push("Another background task is still running...");
    return;
  }
  ctx.notices.setStatus("Syncing with your MyAnimeList and AniList lists...");
}

void startMalLogin(appContext& ctx) {
  bool started = ctx.jobs.start("login", [&ctx]() {
    std::string outcome;
    ctx.accounts.malLogin.authenticate(outcome);
    return outcome;
  });
  if (!started) {
    ctx.notices.push("Another background task is still running...");
    return;
  }
  ctx.notices.setStatus("Waiting for MyAnimeList login in your browser...");
}

bool openAnilistLogin(appContext& ctx) {
  auto loginUrl = ctx.accounts.anilistLogin.authorizeUrl();
  if (!loginUrl) {
    ctx.notices.push(
        "AniList login needs an API client id: register one at "
        "anilist.co/settings/developer and set CATALIST_ANILIST_CLIENT_ID.");
    return false;
  }
  ctx.launcher.spawnDetached(browserOpenCommand(*loginUrl));
  ctx.notices.setStatus("Approve Catalist on AniList, then paste the token here.");
  return true;
}

void finishAnilistLogin(appContext& ctx, const std::string& token) {
  if (!ctx.accounts.anilistLogin.storeToken(token)) {
    ctx.notices.push("No AniList token was pasted.");
    return;
  }
  bool started = ctx.jobs.start("login", [&ctx]() -> std::string {
    auto viewer = ctx.accounts.anilist.viewer();
    if (viewer.ok())
      return "Logged in to AniList as " + viewer.value().name + ".";
    ctx.accounts.anilistLogin.logout();
    return "AniList rejected the token: " + viewer.errorMessage();
  });
  if (!started)
    ctx.notices.push("Another background task is still running...");
}

void exportMalXml(appContext& ctx) {
  auto written = backup::exportMalXml(ctx.fileSystem, ctx.clock, ctx.db);
  if (written.files.empty()) {
    ctx.notices.push("Export failed.");
    return;
  }
  std::string message = "Exported " + std::to_string(written.files.size()) +
                        " MyAnimeList XML files to " +
                        written.files.front().parent_path().string() +
                        " (import them on MyAnimeList or AniList).";
  if (written.skipped > 0)
    message += " " + std::to_string(written.skipped) +
               " entries without a MyAnimeList id were left out.";
  ctx.notices.push(message);
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
  auto reloaded = ctx.db.loadAll();
  ctx.undo.clear();
  if (!reloaded.ok()) {
    ctx.notices.push("Could not read the restored file: " +
                     reloaded.errorMessage());
    return;
  }
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

std::string aboutLine(appContext& ctx) {
  return std::string("Catalist v") + kVersionString + " · " +
         std::to_string(ctx.db.getEntries(mediaType::kAnime).size()) +
         " anime · " +
         std::to_string(ctx.db.getEntries(mediaType::kManga).size()) +
         " manga · data in " + ctx.db.getConfigPath().string();
}

}  // namespace catalist
