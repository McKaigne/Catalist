#include "storage/accounts.hpp"

#include <algorithm>
#include <format>
#include <sstream>

namespace catalist::accounts {

namespace {

std::vector<std::string> fieldsOf(const std::string& line) {
	std::vector<std::string> fields;
	size_t from = 0;
	for (size_t tab = line.find('\t'); tab != std::string::npos; tab = line.find('\t', from)) {
		fields.push_back(line.substr(from, tab - from));
		from = tab + 1;
	}
	fields.push_back(line.substr(from));
	return fields;
}

// A tab or a line break in a field would split the line: none of the sites
// puts one in a name or a token, and one that did is not kept.
bool fits(const account& login) {
	for (const auto& field :
	     {login.user, login.userId, login.token, login.renewal, login.scoreFormat})
		if (field.find_first_of("\t\r\n") != std::string::npos)
			return false;
	return not login.token.empty();
}

}  // namespace

std::filesystem::path defaultPath(const std::string& catalistDataDir,
                                  const std::string& xdgStateHome, const std::string& home) {
	if (not catalistDataDir.empty())
		return std::filesystem::path(catalistDataDir) / "accounts";
	std::filesystem::path state = xdgStateHome;
	if (state.is_absolute())
		return state / "catalist" / "accounts";
	if (not home.empty())
		return std::filesystem::path(home) / ".local" / "state" / "catalist" / "accounts";
	return "catalist-accounts";
}

std::vector<account> parse(const std::string& text) {
	std::vector<account> logins;
	std::istringstream lines(text);
	for (std::string line; std::getline(lines, line);) {
		auto fields = fieldsOf(line);
		// The score format, the 7th field, came later.
		bool known = fields.size() == 6 or fields.size() == 7;
		auto site = known ? parseProvider(fields[0]) : std::nullopt;
		if (not site)
			continue;
		account login{*site, fields[1], fields[2], fields[3], fields[4], 0,
		              fields.size() == 7 ? fields[6] : ""};
		try {
			login.expires = std::stoll(fields[5]);
		} catch (const std::exception&) {
			login.expires = 0;
		}
		if (fits(login) and not on(logins, *site))
			logins.push_back(std::move(login));
	}
	return logins;
}

std::string format(const std::vector<account>& logins) {
	std::string text;
	for (const auto& login : logins)
		if (fits(login))
			text += std::format("{}\t{}\t{}\t{}\t{}\t{}\t{}\n", providerKey(login.site),
			                    login.user, login.userId, login.token, login.renewal,
			                    login.expires, login.scoreFormat);
	return text;
}

std::optional<account> on(const std::vector<account>& logins, provider site) {
	auto found = std::find_if(logins.begin(), logins.end(),
	                          [site](const account& login) { return login.site == site; });
	return found == logins.end() ? std::nullopt : std::optional<account>(*found);
}

std::vector<account> with(std::vector<account> logins, const account& login) {
	logins = without(std::move(logins), login.site);
	logins.push_back(login);
	return logins;
}

std::vector<account> without(std::vector<account> logins, provider site) {
	logins.erase(std::remove_if(logins.begin(), logins.end(),
	                            [site](const account& login) { return login.site == site; }),
	             logins.end());
	return logins;
}

}  // namespace catalist::accounts
