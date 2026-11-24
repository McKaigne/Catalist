#include <clocale>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>

#include "cli/cli.hpp"
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

	catalist::CliContext ctx{
	    db, std::cin, std::cout, std::cerr, catalist::LocalTime, catalist::HttpFetch};
	ctx.interactive = catalist::IsInteractive();
	ctx.columns = catalist::TerminalColumns();
	ctx.rows = catalist::TerminalRows();
	ctx.pager = catalist::ShowInPager;
	ctx.clear_screen = ctx.interactive and env("TERM") != "dumb";
	ctx.color = ctx.columns > 0 and std::getenv("NO_COLOR") == nullptr and env("TERM") != "dumb";
	int code = catalist::RunCli({argv + 1, argv + argc}, ctx);
	// A reader that stopped early is no error; a full disk is.
	std::cout.flush();
	if (std::cout or catalist::OutputIsPipe())
		return code;
	std::cerr << "error: could not write to standard output\n";
	return code != 0 ? code : 1;
}
