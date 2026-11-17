#include <chrono>
#include <clocale>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

#include "cli/cli.hpp"
#include "core/version.hpp"
#include "storage/accounts.hpp"
#include "storage/catalogue.hpp"
#include "system.hpp"

int main(int argc, char* argv[]) {
	std::setlocale(LC_ALL, "");
	// `catalist export anime | head` must end quietly, not die on the signal.
	std::signal(SIGPIPE, SIG_IGN);

	auto env = [](const char* name) {
		const char* value = std::getenv(name);
		return std::string(value ? value : "");
	};
	catalist::DataDirEnv dirs{env("CATALIST_DATA_DIR"), env("XDG_DATA_HOME"), env("HOME")};
	catalist::Catalogue db(catalist::DataDirFor(dirs));

	// A second apart, as the sites ask of programs that send many requests.
	catalist::web::Link net{catalist::HttpFetch,
	                        [] { std::this_thread::sleep_for(std::chrono::seconds(1)); }};
	// The ids the sites gave this build, unless the environment names others.
	auto either = [&env](const char* name, const char* built) {
		std::string given = env(name);
		return given.empty() ? std::string(built) : given;
	};
	net.ids = {either("CATALIST_ANILIST_CLIENT_ID", catalist::kAniListClientId),
	           either("CATALIST_MAL_CLIENT_ID", catalist::kMalClientId),
	           either("CATALIST_KITSU_CLIENT_ID", catalist::kKitsuClientId),
	           either("CATALIST_KITSU_CLIENT_SECRET", catalist::kKitsuClientSecret)};
	net.now = catalist::UnixNow;
	net.random = catalist::RandomText;
	catalist::CliContext ctx{db, std::cin, std::cout, std::cerr, catalist::LocalTime, net};
	ctx.interactive = catalist::IsInteractive();
	ctx.accounts_file = catalist::accounts::DefaultPath(env("CATALIST_DATA_DIR"),
	                                                    env("XDG_STATE_HOME"), env("HOME"));
	ctx.read_secret = catalist::ReadSecretLine;
	ctx.offline = not env("CATALIST_OFFLINE").empty() and env("CATALIST_OFFLINE") != "0";
	ctx.columns = catalist::TerminalColumns();
	// https://no-color.org; a dumb terminal cannot show colours either.
	// CLICOLOR_FORCE asks for them into a pipe as well.
	bool forced = not env("CLICOLOR_FORCE").empty() and env("CLICOLOR_FORCE") != "0";
	ctx.color = (ctx.columns > 0 or forced) and std::getenv("NO_COLOR") == nullptr and
	            env("TERM") != "dumb";
	// The shell's COLUMNS, for --table into a pipe; too narrow to be meant
	// is not taken.
	auto shell_columns = catalist::args::ParseCount(env("COLUMNS"));
	if (shell_columns and *shell_columns >= 20)
		ctx.table_width = *shell_columns;
	return catalist::RunCli({argv + 1, argv + argc}, ctx);
}
