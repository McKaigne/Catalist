#include <clocale>
#include <cstdlib>
#include <filesystem>
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

	// The tests point this at a temporary file, away from the real list.
	const char* test_file = std::getenv("CATALIST_CSV");
	std::filesystem::path file =
	    test_file ? test_file : std::filesystem::path(CATALIST_HOME) / "catalist.csv";
	catalist::Catalogue db(file);
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
