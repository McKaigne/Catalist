#include <clocale>
#include <csignal>
#include <cstdlib>
#include <iostream>

#include "catalogue.hpp"
#include "cli.hpp"
#include "system.hpp"
#include "tui/app.hpp"

int main(int argc, char* argv[]) {
	std::setlocale(LC_ALL, "");
	// `catalist export anime | head` must end quietly, not die on the signal.
	std::signal(SIGPIPE, SIG_IGN);

	catalist::catalogue db(catalist::catalogue::getDefaultConfigDir());
	if (argc > 1) {
		catalist::cliContext ctx{db,        std::cin,        std::cout,
		                         std::cerr, catalist::today, catalist::httpPost};
		ctx.interactive = catalist::isInteractive();
		ctx.columns = catalist::terminalColumns();
		// https://no-color.org; a dumb terminal cannot show colours either.
		const char* term = std::getenv("TERM");
		ctx.color = ctx.columns > 0 and std::getenv("NO_COLOR") == nullptr and
		            not(term and std::string(term) == "dumb");
		ctx.nowStamp = catalist::nowStamp;
		if (const char* group = std::getenv("CATALIST_TX"))
			ctx.txLabel = group;
		return catalist::runCli({argv + 1, argv + argc}, ctx);
	}

	auto loaded = db.loadAll();
	if (not loaded.ok()) {
		std::cerr << "error: " << loaded.errorMessage() << "\n";
		return 1;
	}

	catalist::runTui(db);
	return 0;
}
