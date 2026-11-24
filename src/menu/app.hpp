#ifndef CATALIST_MENU_APP_HPP_
#define CATALIST_MENU_APP_HPP_

#include <ctime>
#include <functional>
#include <istream>
#include <ostream>
#include <string>

#include "core/date.hpp"
#include "storage/catalogue.hpp"
#include "web/http.hpp"

namespace catalist {

struct App {
	Catalogue& db;
	std::istream& in;
	std::ostream& out;
	std::function<std::tm()> clock;
	web::Fetcher net;
	int columns = 80;
	bool color = false;
	bool clear_screen = false;

	[[nodiscard]] IsoDate Today() const { return IsoDate::FromLocalTm(clock()); }
	[[nodiscard]] std::string Stamp() const { return StampOf(clock()); }
};

int RunMenu(App& app);

}  // namespace catalist

#endif  // CATALIST_MENU_APP_HPP_
