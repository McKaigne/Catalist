#include "storage/accounts.hpp"

#include <algorithm>
#include <format>
#include <sstream>

namespace catalist::accounts {

namespace {

std::vector<std::string> FieldsOf(const std::string& line) {
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
bool Fits(const Account& login) {
	for (const auto& field : {login.user, login.user_id, login.token, login.renewal})
		if (field.find_first_of("\t\r\n") != std::string::npos)
			return false;
	return not login.token.empty();
}

}  // namespace

std::filesystem::path DefaultPath(const std::string& catalist_data_dir,
                                  const std::string& xdg_state_home, const std::string& home) {
	if (not catalist_data_dir.empty())
		return std::filesystem::path(catalist_data_dir) / "accounts";
	std::filesystem::path state = xdg_state_home;
	if (state.is_absolute())
		return state / "catalist" / "accounts";
	if (not home.empty())
		return std::filesystem::path(home) / ".local" / "state" / "catalist" / "accounts";
	return "catalist-accounts";
}

std::vector<Account> Parse(const std::string& text) {
	std::vector<Account> logins;
	std::istringstream lines(text);
	for (std::string line; std::getline(lines, line);) {
		auto fields = FieldsOf(line);
		auto site = fields.size() == 6 ? ParseProvider(fields[0]) : std::nullopt;
		if (not site)
			continue;
		Account login{*site, fields[1], fields[2], fields[3], fields[4], 0};
		try {
			login.expires = std::stoll(fields[5]);
		} catch (const std::exception&) {
			login.expires = 0;
		}
		if (Fits(login) and not On(logins, *site))
			logins.push_back(std::move(login));
	}
	return logins;
}

std::string Format(const std::vector<Account>& logins) {
	std::string text;
	for (const auto& login : logins)
		if (Fits(login))
			text += std::format("{}\t{}\t{}\t{}\t{}\t{}\n", ProviderKey(login.site), login.user,
			                    login.user_id, login.token, login.renewal, login.expires);
	return text;
}

std::optional<Account> On(const std::vector<Account>& logins, Provider site) {
	auto found = std::find_if(logins.begin(), logins.end(),
	                          [site](const Account& login) { return login.site == site; });
	return found == logins.end() ? std::nullopt : std::optional<Account>(*found);
}

std::vector<Account> With(std::vector<Account> logins, const Account& login) {
	logins = Without(std::move(logins), login.site);
	logins.push_back(login);
	return logins;
}

std::vector<Account> Without(std::vector<Account> logins, Provider site) {
	logins.erase(std::remove_if(logins.begin(), logins.end(),
	                            [site](const Account& login) { return login.site == site; }),
	             logins.end());
	return logins;
}

}  // namespace catalist::accounts
