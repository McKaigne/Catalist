#include <clocale>
#include <cstdlib>
#include <iostream>
#include <string>

#include "menu/app.hpp"
#include "storage/catalogue.hpp"
#include "system.hpp"

int main(int argc, char*[]) {
	std::setlocale(LC_ALL, "");
	if (argc > 1) {
		std::cerr << "Catalist takes no arguments. Run it and use the menu.\n";
		return 2;
	}

	catalist::Catalogue db(catalist::ProgramDir() / "catalist.csv");
	auto loaded = db.Load();
	if (not loaded.has_value()) {
		std::cerr << "Error: " << loaded.error() << "\n";
		return 1;
	}

	catalist::App app{db, std::cin, std::cout, catalist::LocalTime, catalist::HttpFetch};
	int columns = catalist::TerminalColumns();
	const char* term = std::getenv("TERM");
	bool plain = term and std::string(term) == "dumb";
	app.columns = columns > 0 ? columns : 80;
	app.color = columns > 0 and not plain and std::getenv("NO_COLOR") == nullptr;
	app.clear_screen = catalist::IsInteractive() and not plain;
	return catalist::RunMenu(app);
}
