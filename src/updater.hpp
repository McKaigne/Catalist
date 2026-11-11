#ifndef CATALIST_UPDATER_HPP_
#define CATALIST_UPDATER_HPP_

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "catalogue.hpp"
#include "jikan.hpp"

namespace catalist {

// Keeps titles current from MyAnimeList without making the interface wait:
// a worker thread runs the web requests one at a time, pausing between them
// (Jikan allows three a second), and the interface collects the answers on
// its next tick. Covers are kept in memory only.
class updater {
public:
	// Turns an image into sixel graphics of the given size in pixels.
	using sixelMaker = std::function<std::optional<std::string>(const std::string& image,
	                                                            int widthPx, int heightPx)>;

	// `pause` is the wait between two Jikan requests; a fetch that failed
	// (offline, Jikan busy) may be tried again after `retryAfter`. `wake` is
	// called on the worker thread whenever something finished, so the
	// interface can redraw; never once the updater is being destroyed.
	updater(jikan::fetcher fetch, std::function<isoDate()> today, sixelMaker toSixel,
	        std::chrono::milliseconds pause, std::function<void()> wake = {},
	        std::chrono::milliseconds retryAfter = std::chrono::seconds(15));
	// Waits a moment for a request under way; a slower one is left to finish
	// on its own, so quitting never hangs on the network.
	~updater();
	updater(const updater&) = delete;
	updater& operator=(const updater&) = delete;

	// Looks the title up once in this run; asking again does nothing, unless
	// the earlier lookup could not reach Jikan.
	void lookUp(mediaType type, int malId);

	// What a lookup found.
	struct finding {
		mediaType type;
		int malId;
		webFacts facts;
	};
	// The lookups that finished since the last call.
	[[nodiscard]] std::vector<finding> takeFindings();

	enum class coverState {
		kLoading,
		kReady,
		kUnreachable,  // Jikan or the image could not be fetched; retried later
		kNone,         // MyAnimeList has no cover for the title
		kUnreadable,   // the image could not be turned into sixel
	};
	struct cover {
		coverState state = coverState::kLoading;
		std::string sixel;
	};
	// The title's cover at this size: ready, still on its way (and asked
	// for), or why there is none.
	[[nodiscard]] cover coverFor(mediaType type, int malId, int widthPx, int heightPx);

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

// Asks for a lookup of every followed title (watching or on hold) with a
// MyAnimeList id that has not been checked today.
void checkFollowed(const catalogue& lists, updater& web, const isoDate& today);

}  // namespace catalist

#endif  // CATALIST_UPDATER_HPP_
