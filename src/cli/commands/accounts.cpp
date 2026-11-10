#include "catalist/cli/commands/commands.hpp"

#include <iostream>

#include "catalist/cli/args.hpp"
#include "catalist/cli/output.hpp"
#include "catalist/net/listSync.hpp"

namespace catalist::cli {

namespace {

struct accountOptions {
  iListService* service = nullptr;
  std::optional<std::string> userName;
  std::vector<mediaType> types = {mediaType::kAnime, mediaType::kManga};
};

// Reads "<mal|anilist> [--user NAME] [--type anime|manga]".
std::optional<accountOptions> parseAccountOptions(const commandArgs& args,
                                                  const listAccounts& accounts,
                                                  const std::string& usage) {
  accountOptions options;
  options.service = args.empty() ? nullptr : accounts.find(args[0]);
  if (!options.service) {
    emitError("usage", usage, false);
    return std::nullopt;
  }
  for (size_t i = 1; i < args.size(); ++i) {
    bool hasValue = i + 1 < args.size();
    if ((args[i] == "--user" || args[i] == "-u") && hasValue) {
      options.userName = args[++i];
    } else if ((args[i] == "--type" || args[i] == "-t") && hasValue &&
               parseMediaTypeArgument(args[i + 1])) {
      options.types = {*parseMediaTypeArgument(args[++i])};
    } else {
      emitError("usage", usage, false);
      return std::nullopt;
    }
  }
  return options;
}

int reportOutcome(const std::string& verb, const syncReport& report) {
  std::cout << verb << ": " << report.added << " added, " << report.updated
            << " updated, " << report.pushed << " sent, "
            << report.removedLocally << " removed here, "
            << report.removedRemotely << " removed there.\n";
  for (const auto& problem : report.problems)
    emitError("sync", problem, false);
  return report.ok() ? 0 : 1;
}

std::string connectionLine(const std::string& name, bool connected) {
  return name + ": " + (connected ? "logged in" : "not logged in") + "\n";
}

int loginMal(commandContext& ctx) {
  std::cout << "Opening MyAnimeList in your browser; waiting for the login "
               "(60 seconds)...\n";
  std::string message;
  bool loggedIn = ctx.accounts.malLogin.authenticate(message);
  if (!loggedIn) {
    emitError("login", message, false);
    return 1;
  }
  std::cout << message << "\n";
  return 0;
}

int loginAnilist(commandContext& ctx) {
  auto loginUrl = ctx.accounts.anilistLogin.authorizeUrl();
  if (!loginUrl) {
    emitError("login",
              "AniList login needs an API client id: register one at "
              "https://anilist.co/settings/developer (redirect URL "
              "https://anilist.co/api/v2/oauth/pin) and set "
              "CATALIST_ANILIST_CLIENT_ID.",
              false);
    return 1;
  }
  ctx.launcher.spawnDetached(browserOpenCommand(*loginUrl));
  std::cout << "Approve Catalist on AniList (opened in your browser):\n  "
            << *loginUrl << "\nPaste the token AniList shows: " << std::flush;
  std::string token;
  std::getline(std::cin, token);
  if (!ctx.accounts.anilistLogin.storeToken(token)) {
    emitError("login", "no token was pasted.", false);
    return 1;
  }
  auto viewer = ctx.accounts.anilist.viewer();
  if (!viewer.ok()) {
    ctx.accounts.anilistLogin.logout();
    emitError("login", "AniList rejected the token: " + viewer.errorMessage(),
              false);
    return 1;
  }
  std::cout << "Logged in to AniList as " << viewer.value().name << ".\n";
  return 0;
}

}  // namespace

int runLogin(const commandArgs& args, commandContext& ctx) {
  if (args.empty()) {
    std::cout << connectionLine("MyAnimeList",
                                ctx.accounts.malLogin.isAuthenticated())
              << connectionLine("AniList",
                                ctx.accounts.anilistLogin.isAuthenticated());
    return 0;
  }
  iListService* service = ctx.accounts.find(args[0]);
  if (!service || args.size() > 1) {
    emitError("usage", "usage: catalist login [mal|anilist]", false);
    return 2;
  }
  return service == &ctx.accounts.mal ? loginMal(ctx) : loginAnilist(ctx);
}

int runLogout(const commandArgs& args, commandContext& ctx) {
  iListService* service = args.size() == 1 ? ctx.accounts.find(args[0]) : nullptr;
  if (!service) {
    emitError("usage", "usage: catalist logout <mal|anilist>", false);
    return 2;
  }
  if (service == &ctx.accounts.mal)
    ctx.accounts.malLogin.logout();
  else
    ctx.accounts.anilistLogin.logout();
  std::cout << "Logged out of " << service->displayName() << ".\n";
  return 0;
}

int runSync(const commandArgs& args, commandContext& ctx) {
  std::vector<iListService*> services = ctx.accounts.services();
  if (args.size() > 1 || (args.size() == 1 && !ctx.accounts.find(args[0]))) {
    emitError("usage", "usage: catalist sync [mal|anilist]", false);
    return 2;
  }
  if (args.size() == 1)
    services = {ctx.accounts.find(args[0])};
  listSync sync(ctx.db, ctx.clock, services, &ctx.accounts.anilist);
  return reportOutcome("Sync", sync.run());
}

int importFromAccount(const commandArgs& args, commandContext& ctx) {
  auto options = parseAccountOptions(
      args, ctx.accounts,
      "usage: catalist import <mal|anilist> [--user NAME] [--type anime|manga]");
  if (!options)
    return 2;
  bool needsLogin = !options->userName && !options->service->isConnected();
  if (needsLogin) {
    emitError("login",
              "log in first ('catalist login " + options->service->key() +
                  "') or name a public list with --user.",
              false);
    return 1;
  }
  listSync sync(ctx.db, ctx.clock, {options->service}, nullptr);
  syncReport total;
  for (auto type : options->types) {
    auto report = sync.importFrom(*options->service, type, options->userName);
    total.added += report.added;
    total.updated += report.updated;
    total.problems.insert(total.problems.end(), report.problems.begin(),
                          report.problems.end());
  }
  return reportOutcome("Import", total);
}

}  // namespace catalist::cli
