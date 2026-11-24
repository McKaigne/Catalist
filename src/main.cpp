#include <clocale>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include "menu/app.hpp"
#include "storage/catalogue.hpp"
#include "system.hpp"

namespace {

std::filesystem::path ListFile() {
	// The tests point this at a temporary file, away from the real list.
	if (const char* test_file = std::getenv("CATALIST_CSV"))
		return test_file;
	// An installed copy's build folder is gone: use the folder it is run from.
	std::filesystem::path home = CATALIST_HOME;
	std::error_code ignored;
	if (not std::filesystem::is_directory(home, ignored))
		home = std::filesystem::current_path();
	return home / "catalist.csv";
}

}  // namespace

int main(int argc, char*[]) {
	std::setlocale(LC_ALL, "");
	if (argc > 1) {
		std::cerr << "Catalist takes no arguments. Run it and use the menu.\n";
		return 2;
	}

	catalist::Catalogue db(ListFile());
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
