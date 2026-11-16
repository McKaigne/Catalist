#include <clocale>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include "catalogue.hpp"
#include "cli.hpp"
#include "system.hpp"
#include "tui/app.hpp"

int main(int argc, char* argv[]) {
	std::setlocale(LC_ALL, "");
	// `catalist export anime | head` must end quietly, not die on the signal.
	std::signal(SIGPIPE, SIG_IGN);

	auto env = [](const char* name) {
		const char* value = std::getenv(name);
		return std::string(value ? value : "");
	};
	catalist::dataDirEnv dirs{env("CATALIST_DATA_DIR"), env("XDG_DATA_HOME"),
	                          env("XDG_CONFIG_HOME"), env("HOME")};
	catalist::catalogue db(catalist::catalogue::getDefaultDataDir(dirs),
	                       catalist::catalogue::getOldDataDir(dirs));
	// `cl` is the same program under a shorter name.
	std::string name = std::filesystem::path(argv[0]).filename().string();
	if (name.empty())
		name = "catalist";
	if (argc > 1) {
		catalist::cliContext ctx{db,        std::cin,        std::cout,
		                         std::cerr, catalist::today, catalist::httpPost};
		ctx.interactive = catalist::isInteractive();
		ctx.columns = catalist::terminalColumns();
		// https://no-color.org; a dumb terminal cannot show colours either.
		ctx.color =
		    ctx.columns > 0 and std::getenv("NO_COLOR") == nullptr and env("TERM") != "dumb";
		ctx.nowStamp = catalist::nowStamp;
		ctx.txLabel = env("CATALIST_TX");
		ctx.newTxLabel = catalist::newTxLabel;
		ctx.program = name;
		return catalist::runCli({argv + 1, argv + argc}, ctx);
	}

	// The tracker draws on a terminal and reads its keys from one. In a
	// pipe or a script, a command was meant.
	if (not catalist::isInteractive()) {
		std::cerr << catalist::noTerminalNote(name);
		return 2;
	}

	auto loaded = db.loadAll();
	if (not loaded.ok()) {
		std::cerr << "error: " << loaded.errorMessage() << "\n";
		return 1;
	}
	std::cerr << catalist::importNote(db);

	catalist::runTui(db);
	return 0;
}
