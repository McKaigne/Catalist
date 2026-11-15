#ifndef CATALIST_UPDATER_HPP_
#define CATALIST_UPDATER_HPP_

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "anilist.hpp"
#include "catalogue.hpp"

namespace catalist {

// Keeps titles current from AniList without making the interface wait: a
// worker thread runs the web requests one at a time, pausing between them,
// and the interface collects the answers on its next tick. Covers are kept
// in memory only.
class updater {
public:
	// Fetches a URL (an image here): the body, or nothing when it failed.
	using fetcher = std::function<std::optional<std::string>(const std::string&)>;
	// Turns an image into sixel graphics of the given size in pixels.
	using sixelMaker = std::function<std::optional<std::string>(const std::string& image,
	                                                            int widthPx, int heightPx)>;

	// `pause` is the wait between two AniList requests; a fetch that failed
	// (offline, AniList busy) may be tried again after `retryAfter`. `wake` is
	// called on the worker thread whenever something finished, so the
	// interface can redraw; never once the updater is being destroyed.
	updater(fetcher fetch, anilist::poster post, std::function<isoDate()> today, sixelMaker toSixel,
	        std::chrono::milliseconds pause, std::function<void()> wake = {},
	        std::chrono::milliseconds retryAfter = std::chrono::seconds(15));
	// Waits a moment for a request under way; a slower one is left to finish
	// on its own, so quitting never hangs on the network.
	~updater();
	updater(const updater&) = delete;
	updater& operator=(const updater&) = delete;

	// Looks the title up once in this run; asking again does nothing, unless
	// the earlier lookup could not reach AniList. A title with no id is not
	// looked up.
	void lookUp(const anilist::titleRef& title);

	// What a lookup found: the title as asked for, and the facts, which
	// carry both its ids.
	struct finding {
		anilist::titleRef asked;
		webFacts facts;
	};
	// The lookups that finished since the last call.
	[[nodiscard]] std::vector<finding> takeFindings();

	// Asks for both ids of titles of one type known by their ids on `known`.
	void linkIds(mediaType type, provider known, std::vector<int> ids);
	// What the id lookups found, per type, since the last call.
	struct link {
		mediaType type;
		std::vector<anilist::idPair> ids;
	};
	[[nodiscard]] std::vector<link> takeLinks();

	enum class coverState {
		kLoading,
		kReady,
		kUnreachable,  // AniList or the image could not be fetched; retried later
		kNone,         // the title has no id, or AniList no cover for it
		kUnreadable,   // the image could not be turned into sixel
	};
	struct cover {
		coverState state = coverState::kLoading;
		std::string sixel;
	};
	// The title's cover at this size: ready, still on its way (and asked
	// for), or why there is none.
	[[nodiscard]] cover coverFor(const anilist::titleRef& title, int widthPx, int heightPx);

	// Blocks until every queued request is done; for tests.
	void waitUntilIdle();

private:
	// Everything the worker thread uses. It is shared with the thread, so a
	// worker still waiting on a slow request when the program quits can
	// finish on its own without touching a destroyed updater.
	struct shared;
	std::shared_ptr<shared> shared_;
	std::thread worker_;
};

// Asks for a lookup of every followed title (watching or on hold) with an
// id that has not been checked today.
void checkFollowed(const catalogue& lists, updater& web, const isoDate& today);

// Titles of one type that lack one id but have the other, as the ids to ask
// by: MyAnimeList ids of titles without an AniList id, and AniList ids of
// titles without a MyAnimeList one (some never have one).
struct missingIds {
	mediaType type;
	provider known;
	std::vector<int> ids;
};
[[nodiscard]] std::vector<missingIds> findMissingIds(const catalogue& lists);

// Fills in what an id lookup found; returns how many titles gained an id.
int fillIds(catalogue& lists, mediaType type, const std::vector<anilist::idPair>& found);

}  // namespace catalist

#endif  // CATALIST_UPDATER_HPP_
