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
std::optional<Provider> SiteOperand(const Command& self, const args::Parsed& line,
                                    CliContext& ctx) {
	if (line.operands.size() != 1) {
		UsageError(ctx, self, "name one site: anilist, mal or kitsu");
		return std::nullopt;
	}
	auto site = ParseProvider(line.operands[0]);
	if (not site)
		UsageError(ctx, self,
		           std::format("unknown site '{}'{}; choose anilist, mal or kitsu",
		                       line.operands[0], DidYouMean(line.operands[0], kSiteWords)));
	return site;
}

// The day a login runs out, from seconds since 1970 (UTC).
std::string DayOf(long long seconds) {
	if (seconds <= 0)
		return "-";
	std::chrono::sys_days day{std::chrono::days{seconds / 86400}};
	return std::format("{:%F}", day);
}

std::string Trimmed(std::string text) {
	text.erase(0, text.find_first_not_of(" \t\r"));
	text.erase(text.find_last_not_of(" \t\r") + 1);
	return text;
}

int RunLogin(const Command& self, const args::Parsed& line, CliContext& ctx) {
	auto site = SiteOperand(self, line, ctx);
	if (not site)
		return 2;
	auto tracker = TrackerFor(ctx, *site);
	auto start = tracker->StartLogin();
	if (not start.has_value())
		return Fail(ctx, start.error().message);

	ctx.err << start.value().explanation << "\n";
	if (not start.value().page.empty())
		ctx.err << "  " << start.value().page << "\n";
	std::vector<std::string> answers;
	for (const auto& each : start.value().questions) {
		ctx.err << each.ask << ": " << std::flush;
		std::optional<std::string> answer;
		std::string typed;
		if (each.secret)
			answer = ctx.read_secret(ctx.in);
		else if (std::getline(ctx.in, typed))
			answer = typed;
		if (not answer or Trimmed(*answer).empty()) {
			ctx.err << "Cancelled.\n";
			return 1;
		}
		answers.push_back(Trimmed(*answer));
	}
	auto login = tracker->FinishLogin(answers);
	if (not login.has_value())
		return Fail(ctx, login.error().message);
	if (not SaveAccounts(ctx, accounts::With(ReadAccounts(ctx), login.value())))
		return Fail(ctx, std::format("could not save the login to {}", ctx.accounts_file.string()));
	ctx.err << std::format(
	    "Logged in to {} as {}. Each change to a linked title is now sent there;\n"
	    "send your lists there now with: catalist sync --all\n",
	    ProviderName(*site), login.value().user);
	return 0;
}

int RunLogout(const Command& self, const args::Parsed& line, CliContext& ctx) {
	auto site = SiteOperand(self, line, ctx);
	if (not site)
		return 2;
	auto logins = ReadAccounts(ctx);
	if (not accounts::On(logins, *site)) {
		ctx.err << std::format("You are not logged in to {}.\n", ProviderName(*site));
		return 0;
	}
	if (not SaveAccounts(ctx, accounts::Without(logins, *site)))
		return Fail(ctx, std::format("could not save {}", ctx.accounts_file.string()));
	ctx.err << std::format("Logged out of {}.\n", ProviderName(*site));
	return 0;
}

int RunAccounts(const Command& self, const args::Parsed& line, CliContext& ctx) {
	if (not line.operands.empty())
		return UsageError(ctx, self, "accounts takes no operands");
	auto logins = ReadAccounts(ctx);
	long long now = ctx.net.now();
	auto state_of = [now](const Account& login) {
		return login.expires > 0 and login.expires <= now ? "run out" : "in";
	};
	if (not line.Has("json") and ctx.columns > 0) {
		if (logins.empty()) {
			ctx.err << "You are not logged in anywhere. Log in with: catalist login SITE\n";
			return 0;
		}
		std::vector<std::vector<output::Styled>> rows;
		for (const auto& login : logins)
			rows.push_back({{ProviderKey(login.site), output::kCyan},
			                {login.user},
			                {state_of(login), login.expires > 0 and login.expires <= now
			                                      ? output::kRed
			                                      : static_cast<const char*>(nullptr)},
			                {DayOf(login.expires)}});
		ctx.out << output::Table({{"SITE"}, {"USER", false, true}, {"STATE"}, {"UNTIL"}}, rows,
		                         ctx.columns, ctx.color);
		return 0;
	}
	std::vector<std::vector<output::Cell>> rows;
	for (const auto& login : logins)
		rows.push_back({output::Text(ProviderKey(login.site)), output::Text(login.user),
		                output::Text(state_of(login)), output::Maybe(DayOf(login.expires))});
	WriteRows(ctx, {"site", "user", "state", "until"}, rows, line.Has("json"), line.Has("header"));
	return 0;
}

}  // namespace

std::vector<Command> AccountCommands() {
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
	     .run = RunLogin,
	     .completes = OperandKind::kWords,
	     .words = kSiteWords},
	    {.name = "logout",
	     .summary = "Forget the login of a site",
	     .details = "Forgets the token Catalist keeps for the site. The site may still list\n"
	                "Catalist among your apps: take it off there too, if you like.\n",
	     .operands = "anilist|mal|kitsu",
	     .run = RunLogout,
	     .completes = OperandKind::kWords,
	     .words = kSiteWords},
	    {.name = "accounts",
	     .summary = "List the sites you are logged in to",
	     .details = "Into a pipe each login is one line of fields with a tab between them:\n"
	                "site, user, state (in, or run out) and until (the day its token runs\n"
	                "out, or '-' when it does not say). Tokens are never printed.\n",
	     .options = {{0, "json", "", "print one JSON object a line"},
	                 {0, "header", "", "name the columns on the first line of a pipe"}},
	     .run = RunAccounts},
	};
}

}  // namespace catalist::cli
