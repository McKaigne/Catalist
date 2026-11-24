#ifndef CATALIST_CLI_CLI_HPP_
#define CATALIST_CLI_CLI_HPP_

#include <ctime>
#include <functional>
#include <istream>
#include <string>
#include <vector>

#include "cli/args.hpp"
#include "core/date.hpp"
#include "storage/catalogue.hpp"
#include "web/http.hpp"

namespace catalist {

struct CliContext {
	Catalogue& db;
	std::istream& in;
	std::ostream& out;
	std::ostream& err;
	std::function<std::tm()> clock;
	web::Fetcher net;
	bool interactive = false;
	bool color = false;
	int columns = 0;
	int rows = 0;
	std::function<bool(const std::string&)> pager{};
	std::string typed{};
	// Names the undo step instead of `typed`, for a command the menu built.
	std::string label{};
	bool clear_screen = false;

	[[nodiscard]] IsoDate Today() const { return IsoDate::FromLocalTm(clock()); }
	[[nodiscard]] std::string Stamp() const { return StampOf(clock()); }
};

struct Command {
	std::string name{};
	std::vector<std::string> aliases{};
	std::string summary{};
	std::string details{};
	std::vector<args::Option> options{};
	std::string operands{};
	std::vector<WatchStatus> pick_from{};
	int (*run)(const Command& self, const args::Parsed& line, CliContext& ctx) = nullptr;
};

[[nodiscard]] const std::vector<Command>& Commands();
[[nodiscard]] std::string UsageLine(const Command& shown);
[[nodiscard]] const std::vector<args::Option>& GlobalOptions();
[[nodiscard]] const args::Option& HelpOption();

int RunCli(const std::vector<std::string>& args, CliContext& ctx);

}  // namespace catalist

#endif  // CATALIST_CLI_CLI_HPP_
