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
[[nodiscard]] std::filesystem::path defaultPath(const std::string& catalistDataDir,
                                                const std::string& xdgStateHome,
                                                const std::string& home);

// The logins in the text; lines it cannot read are left out.
[[nodiscard]] std::vector<account> parse(const std::string& text);
[[nodiscard]] std::string format(const std::vector<account>& logins);

// The login on a site, when there is one.
[[nodiscard]] std::optional<account> on(const std::vector<account>& logins, provider site);
// The logins with `login` in place of the one on its site.
[[nodiscard]] std::vector<account> with(std::vector<account> logins, const account& login);
[[nodiscard]] std::vector<account> without(std::vector<account> logins, provider site);

}  // namespace catalist::accounts

#endif  // CATALIST_STORAGE_ACCOUNTS_HPP_
