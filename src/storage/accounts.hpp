#ifndef CATALIST_STORAGE_ACCOUNTS_HPP_
#define CATALIST_STORAGE_ACCOUNTS_HPP_

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "core/account.hpp"

// The logins, kept in a file of their own that only its owner can read, and
// never in catalist.db: the lists can be copied and shared, the logins not.
// One line a site: site, user, user id, token, renewal and expiry, a tab
// between them. Pure text in and out; system.cpp writes the file.
namespace catalist::accounts {

// $CATALIST_DATA_DIR/accounts when that is set; else
// $XDG_STATE_HOME/catalist/accounts, or ~/.local/state/catalist/accounts.
[[nodiscard]] std::filesystem::path DefaultPath(const std::string& catalist_data_dir,
                                                const std::string& xdg_state_home,
                                                const std::string& home);

// The logins in the text; lines it cannot read are left out.
[[nodiscard]] std::vector<Account> Parse(const std::string& text);
[[nodiscard]] std::string Format(const std::vector<Account>& logins);

// The login on a site, when there is one.
[[nodiscard]] std::optional<Account> On(const std::vector<Account>& logins, Provider site);
// The logins with `login` in place of the one on its site.
[[nodiscard]] std::vector<Account> With(std::vector<Account> logins, const Account& login);
[[nodiscard]] std::vector<Account> Without(std::vector<Account> logins, Provider site);

}  // namespace catalist::accounts

#endif  // CATALIST_STORAGE_ACCOUNTS_HPP_
