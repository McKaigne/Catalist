#ifndef CATALIST_UPDATER_HPP_
#define CATALIST_UPDATER_HPP_

#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <tuple>
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

	// `wake` is called on the worker thread whenever something finished, so
	// the interface can redraw.
	updater(jikan::fetcher fetch, std::function<isoDate()> today, sixelMaker toSixel,
	        std::chrono::milliseconds pause, std::function<void()> wake = {});
	~updater();
	updater(const updater&) = delete;
	updater& operator=(const updater&) = delete;

	// Looks the title up once in this run; asking again does nothing.
	void lookUp(mediaType type, int malId);

	// What a lookup found.
	struct finding {
		mediaType type;
		int malId;
		webFacts facts;
	};
	// The lookups that finished since the last call.
	[[nodiscard]] std::vector<finding> takeFindings();

	enum class coverState { kLoading, kReady, kNone };
	struct cover {
		coverState state = coverState::kLoading;
		std::string sixel;
	};
	// The title's cover at this size: ready, still on its way (and asked
	// for), or not to be had.
	[[nodiscard]] cover coverFor(mediaType type, int malId, int widthPx, int heightPx);

	// Blocks until every queued request is done; for tests.
	void waitUntilIdle();

private:
	using titleKey = std::pair<mediaType, int>;
	using coverKey = std::tuple<mediaType, int, int, int>;
	struct job {
		bool isCover = false;
		coverKey key;
	};

	void work();
	void runLookup(const titleKey& title);
	void runCover(const coverKey& key);
	std::optional<std::string> pacedFetch(const std::string& url);
	std::optional<std::string> imageUrlOf(const titleKey& title);

	jikan::fetcher fetch_;
	std::function<isoDate()> today_;
	sixelMaker toSixel_;
	std::chrono::milliseconds pause_;
	std::function<void()> wake_;

	// Everything below is shared with the worker thread and guarded by mutex_.
	std::mutex mutex_;
	std::condition_variable changed_;
	std::deque<job> jobs_;
	bool working_ = false;
	bool stopping_ = false;
	std::chrono::steady_clock::time_point lastRequest_;
	std::set<titleKey> lookedUp_;
	std::vector<finding> findings_;
	std::map<titleKey, std::string> imageUrls_;
	std::map<titleKey, std::string> images_;
	std::set<titleKey> noImage_;
	std::set<coverKey> asked_;
	std::map<coverKey, std::string> sixels_;

	std::thread worker_;
};

// Asks for a lookup of every followed title (watching or on hold) with a
// MyAnimeList id that has not been checked today.
void checkFollowed(const catalogue& lists, updater& web, const isoDate& today);

}  // namespace catalist

#endif  // CATALIST_UPDATER_HPP_
