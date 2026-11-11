#include "updater.hpp"

#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <set>
#include <tuple>

namespace catalist {

namespace {

// How long quitting waits for a request under way.
constexpr auto kStopWait = std::chrono::milliseconds(200);

using titleKey = std::pair<mediaType, int>;
using coverKey = std::tuple<mediaType, int, int, int>;

struct job {
	bool isCover = false;
	coverKey key;
};

}  // namespace

struct updater::shared {
	shared(jikan::fetcher fetch, std::function<isoDate()> today, sixelMaker toSixel,
	       std::chrono::milliseconds pause, std::function<void()> wake)
	    : fetch(std::move(fetch)),
	      today(std::move(today)),
	      toSixel(std::move(toSixel)),
	      pause(pause),
	      wake(std::move(wake)) {}

	void work();
	void runLookup(const titleKey& title);
	void runCover(const coverKey& key);
	std::optional<std::string> pacedFetch(const std::string& url);
	std::optional<std::string> imageUrlOf(const titleKey& title);

	const jikan::fetcher fetch;
	const std::function<isoDate()> today;
	const sixelMaker toSixel;
	const std::chrono::milliseconds pause;
	const std::function<void()> wake;

	// Everything below is guarded by mutex.
	std::mutex mutex;
	std::condition_variable changed;
	std::deque<job> jobs;
	bool working = false;
	bool stopping = false;
	bool done = false;
	std::chrono::steady_clock::time_point lastRequest;
	std::set<titleKey> lookedUp;
	std::vector<finding> findings;
	std::map<titleKey, std::string> imageUrls;
	std::map<titleKey, std::string> images;
	std::set<titleKey> noImage;
	std::set<coverKey> asked;
	std::map<coverKey, std::string> sixels;
};

updater::updater(jikan::fetcher fetch, std::function<isoDate()> today, sixelMaker toSixel,
                 std::chrono::milliseconds pause, std::function<void()> wake)
    : shared_(std::make_shared<shared>(std::move(fetch), std::move(today), std::move(toSixel),
                                       pause, std::move(wake))) {
	worker_ = std::thread([state = shared_] { state->work(); });
}

updater::~updater() {
	std::unique_lock<std::mutex> lock(shared_->mutex);
	shared_->stopping = true;
	shared_->changed.notify_all();
	// A request under way may take seconds (curl allows eight). After a
	// moment the worker is left to finish it alone; it holds its own share
	// of the state and calls nothing outside it once stopping is set.
	bool finished = shared_->changed.wait_for(lock, kStopWait, [this] { return shared_->done; });
	lock.unlock();
	if (finished)
		worker_.join();
	else
		worker_.detach();
}

void updater::lookUp(mediaType type, int malId) {
	{
		std::lock_guard<std::mutex> lock(shared_->mutex);
		if (malId <= 0 or not shared_->lookedUp.insert({type, malId}).second)
			return;
		shared_->jobs.push_back({false, {type, malId, 0, 0}});
	}
	shared_->changed.notify_all();
}

std::vector<updater::finding> updater::takeFindings() {
	std::lock_guard<std::mutex> lock(shared_->mutex);
	return std::exchange(shared_->findings, {});
}

updater::cover updater::coverFor(mediaType type, int malId, int widthPx, int heightPx) {
	coverKey key{type, malId, widthPx, heightPx};
	{
		std::lock_guard<std::mutex> lock(shared_->mutex);
		auto ready = shared_->sixels.find(key);
		if (ready != shared_->sixels.end())
			return {coverState::kReady, ready->second};
		if (malId <= 0 or shared_->noImage.count({type, malId}))
			return {coverState::kNone, ""};
		if (not shared_->asked.insert(key).second)
			return {coverState::kLoading, ""};
		// The person is looking at this one, so it goes before any lookups.
		shared_->jobs.push_front({true, key});
	}
	shared_->changed.notify_all();
	return {coverState::kLoading, ""};
}

void updater::waitUntilIdle() {
	std::unique_lock<std::mutex> lock(shared_->mutex);
	shared_->changed.wait(lock, [this] { return shared_->jobs.empty() and not shared_->working; });
}

void updater::shared::work() {
	while (true) {
		// Take the next job, or stop.
		job next;
		{
			std::unique_lock<std::mutex> lock(mutex);
			changed.wait(lock, [this] { return stopping or not jobs.empty(); });
			if (stopping) {
				done = true;
				changed.notify_all();
				return;
			}
			next = jobs.front();
			jobs.pop_front();
			working = true;
		}

		// The requests run without the lock, so the interface never waits.
		if (next.isCover)
			runCover(next.key);
		else
			runLookup({std::get<0>(next.key), std::get<1>(next.key)});

		// Report back; the interface is only woken while it still exists.
		std::lock_guard<std::mutex> lock(mutex);
		working = false;
		changed.notify_all();
		if (wake and not stopping)
			wake();
	}
}

std::optional<std::string> updater::shared::pacedFetch(const std::string& url) {
	{
		std::unique_lock<std::mutex> lock(mutex);
		changed.wait_until(lock, lastRequest + pause, [this] { return stopping; });
		if (stopping)
			return std::nullopt;
		lastRequest = std::chrono::steady_clock::now();
	}
	return fetch(url);
}

void updater::shared::runLookup(const titleKey& title) {
	auto paced = [this](const std::string& url) { return pacedFetch(url); };
	auto found = jikan::lookUp(paced, title.first, title.second, today());
	if (not found.ok())
		return;
	std::lock_guard<std::mutex> lock(mutex);
	if (not found.value().imageUrl.empty())
		imageUrls[title] = found.value().imageUrl;
	findings.push_back({title.first, title.second, found.value().facts});
}

std::optional<std::string> updater::shared::imageUrlOf(const titleKey& title) {
	{
		std::lock_guard<std::mutex> lock(mutex);
		auto known = imageUrls.find(title);
		if (known != imageUrls.end())
			return known->second;
	}
	auto body = pacedFetch(jikan::detailsUrl(title.first, title.second));
	auto found = body ? jikan::parseDetails(*body, title.first)
	                  : result<jikan::details>::failure("unreachable");
	if (not found.ok() or found.value().imageUrl.empty())
		return std::nullopt;
	std::lock_guard<std::mutex> lock(mutex);
	imageUrls[title] = found.value().imageUrl;
	return found.value().imageUrl;
}

void updater::shared::runCover(const coverKey& key) {
	auto [type, malId, widthPx, heightPx] = key;
	titleKey title{type, malId};

	// The image itself is fetched once; each size is converted from it.
	std::optional<std::string> image;
	{
		std::lock_guard<std::mutex> lock(mutex);
		auto known = images.find(title);
		if (known != images.end())
			image = known->second;
	}
	if (not image) {
		auto url = imageUrlOf(title);
		image = url ? pacedFetch(*url) : std::nullopt;
	}
	auto sixel = image ? toSixel(*image, widthPx, heightPx) : std::nullopt;

	std::lock_guard<std::mutex> lock(mutex);
	if (image)
		images[title] = *image;
	if (sixel)
		sixels[key] = *sixel;
	else
		noImage.insert(title);
}

void checkFollowed(const catalogue& lists, updater& web, const isoDate& today) {
	for (mediaType type : {mediaType::kAnime, mediaType::kManga}) {
		for (const auto& e : lists.getEntries(type)) {
			bool following = e.status == watchStatus::kWatching or e.status == watchStatus::kOnHold;
			if (following and e.malId > 0 and e.checkedAt != today)
				web.lookUp(type, e.malId);
		}
	}
}

}  // namespace catalist
