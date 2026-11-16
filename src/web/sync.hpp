#ifndef CATALIST_WEB_SYNC_HPP_
#define CATALIST_WEB_SYNC_HPP_

#include <vector>

#include "core/entry.hpp"
#include "core/result.hpp"

namespace catalist {

// Keeping the catalogue in step with a list on the chosen provider's site.
// Nothing implements this yet: it marks where a sync plugs in, so adding one
// touches no other module's design.
//
// The plan, for whoever writes the first one:
// - One class per site (anilistSync over AniList's GraphQL mutations,
//   malSync over MyAnimeList's official v2 API), each signed in with the
//   account's own token, which is kept out of the catalogue files.
// - Only the provider in the catalogue's settings is synced; changing
//   provider switches which class is made, never runs two.
// - Entries already carry both sites' ids, so a sync matches titles by
//   idOn(entry, site()) and needs no new columns.
// - A command that changes titles would push them after its save, and a
//   `sync` command would pull; both through the anilist::poster style of
//   function, so tests swap the network for a fake.
class iListSync {
public:
	virtual ~iListSync() = default;

	[[nodiscard]] virtual provider site() const = 0;
	// The account's whole list of one type, as entries.
	[[nodiscard]] virtual result<std::vector<entry>> pull(mediaType type) = 0;
	// Sends one changed or added entry.
	[[nodiscard]] virtual result<bool> push(const entry& changed) = 0;
	// Takes the title off the account's list.
	[[nodiscard]] virtual result<bool> remove(const entry& gone) = 0;
};

}  // namespace catalist

#endif  // CATALIST_WEB_SYNC_HPP_
