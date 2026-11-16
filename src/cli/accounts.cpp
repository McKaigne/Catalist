#include "storage/accounts.hpp"

#include <chrono>
#include <format>
#include <istream>
#include <ostream>

#include "cli/common.hpp"
#include "system.hpp"

namespace catalist::cli {

namespace {

const std::vector<std::string> kSiteWords = {"anilist", "mal", "kitsu"};

// The site the one operand names. Nothing, after the usage error, when it
// names none.
std::optional<provider> siteOperand(const command& self, const args::parsed& line,
                                    cliContext& ctx) {
	if (line.operands.size() != 1) {
		usageError(ctx, self, "name one site: anilist, mal or kitsu");
		return std::nullopt;
	}
	auto site = parseProvider(line.operands[0]);
	if (not site)
		usageError(ctx, self,
		           std::format("unknown site '{}'{}; choose anilist, mal or kitsu",
		                       line.operands[0], didYouMean(line.operands[0], kSiteWords)));
	return site;
}

// The day a login runs out, from seconds since 1970 (UTC).
std::string dayOf(long long seconds) {
	if (seconds <= 0)
		return "-";
	std::chrono::sys_days day{std::chrono::days{seconds / 86400}};
	return std::format("{:%F}", day);
}

std::string trimmed(std::string text) {
	text.erase(0, text.find_first_not_of(" \t\r"));
	text.erase(text.find_last_not_of(" \t\r") + 1);
	return text;
}

int runLogin(const command& self, const args::parsed& line, cliContext& ctx) {
	auto site = siteOperand(self, line, ctx);
	if (not site)
		return 2;
	auto tracker = trackerFor(ctx, *site);
	auto start = tracker->startLogin();
	if (not start.has_value())
		return fail(ctx, start.error().message);

	ctx.err << start.value().explanation << "\n";
	if (not start.value().page.empty())
		ctx.err << "  " << start.value().page << "\n";
	std::vector<std::string> answers;
	for (const auto& each : start.value().questions) {
		ctx.err << each.ask << ": " << std::flush;
		std::optional<std::string> answer;
		std::string typed;
		if (each.secret)
			answer = ctx.readSecret(ctx.in);
		else if (std::getline(ctx.in, typed))
			answer = typed;
		if (not answer or trimmed(*answer).empty()) {
			ctx.err << "Cancelled.\n";
			return 1;
		}
		answers.push_back(trimmed(*answer));
	}
	auto login = tracker->finishLogin(answers);
	if (not login.has_value())
		return fail(ctx, login.error().message);
	if (not saveAccounts(ctx, accounts::with(readAccounts(ctx), login.value())))
		return fail(ctx, std::format("could not save the login to {}", ctx.accountsFile.string()));
	ctx.err << std::format(
	    "Logged in to {} as {}. Each change to a linked title is now sent there;\n"
	    "send your lists there now with: catalist sync --all\n",
	    providerName(*site), login.value().user);
	return 0;
}

int runLogout(const command& self, const args::parsed& line, cliContext& ctx) {
	auto site = siteOperand(self, line, ctx);
	if (not site)
		return 2;
	auto logins = readAccounts(ctx);
	if (not accounts::on(logins, *site)) {
		ctx.err << std::format("You are not logged in to {}.\n", providerName(*site));
		return 0;
	}
	if (not saveAccounts(ctx, accounts::without(logins, *site)))
		return fail(ctx, std::format("could not save {}", ctx.accountsFile.string()));
	ctx.err << std::format("Logged out of {}.\n", providerName(*site));
	return 0;
}

int runAccounts(const command& self, const args::parsed& line, cliContext& ctx) {
	if (not line.operands.empty())
		return usageError(ctx, self, "accounts takes no operands");
	auto logins = readAccounts(ctx);
	long long now = ctx.net.now();
	auto stateOf = [now](const account& login) {
		return login.expires > 0 and login.expires <= now ? "run out" : "in";
	};
	if (not line.has("json") and ctx.columns > 0) {
		if (logins.empty()) {
			ctx.err << "You are not logged in anywhere. Log in with: catalist login SITE\n";
			return 0;
		}
		std::vector<std::vector<output::styled>> rows;
		for (const auto& login : logins)
			rows.push_back({{providerKey(login.site), output::kCyan},
			                {login.user},
			                {stateOf(login), login.expires > 0 and login.expires <= now
			                                     ? output::kRed
			                                     : static_cast<const char*>(nullptr)},
			                {dayOf(login.expires)}});
		ctx.out << output::table({{"SITE"}, {"USER", false, true}, {"STATE"}, {"UNTIL"}}, rows,
		                         ctx.columns, ctx.color);
		return 0;
	}
	std::vector<std::vector<output::cell>> rows;
	for (const auto& login : logins)
		rows.push_back({output::text(providerKey(login.site)), output::text(login.user),
		                output::text(stateOf(login)), output::maybe(dayOf(login.expires))});
	writeRows(ctx, {"site", "user", "state", "until"}, rows, line.has("json"), line.has("header"));
	return 0;
}

}  // namespace

std::vector<command> accountCommands() {
	return {
	    {.name = "login",
	     .summary = "Log in to a site",
	     .details = "Logs in to AniList, MyAnimeList or Kitsu, so that Catalist can later\n"
	                "tell it what changes in your lists. Each site has its own way, which\n"
	                "the command walks you through: AniList shows a token to paste back,\n"
	                "MyAnimeList sends your browser to an address to paste back, and Kitsu\n"
	                "asks for your email and password (not kept: Kitsu gives a token for\n"
	                "them). Logging in again replaces the login. Logins are kept, readable\n"
	                "by you only, apart from your lists (see FILES in the manual).\n",
	     .operands = "anilist|mal|kitsu",
	     .run = runLogin,
	     .completes = operandKind::kWords,
	     .words = kSiteWords},
	    {.name = "logout",
	     .summary = "Forget the login of a site",
	     .details = "Forgets the token Catalist keeps for the site. The site may still list\n"
	                "Catalist among your apps: take it off there too, if you like.\n",
	     .operands = "anilist|mal|kitsu",
	     .run = runLogout,
	     .completes = operandKind::kWords,
	     .words = kSiteWords},
	    {.name = "accounts",
	     .summary = "List the sites you are logged in to",
	     .details = "Into a pipe each login is one line of fields with a tab between them:\n"
	                "site, user, state (in, or run out) and until (the day its token runs\n"
	                "out, or '-' when it does not say). Tokens are never printed.\n",
	     .options = {{0, "json", "", "print one JSON object a line"},
	                 {0, "header", "", "name the columns on the first line of a pipe"}},
	     .run = runAccounts},
	};
}

}  // namespace catalist::cli
