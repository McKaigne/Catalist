#ifndef CATALIST_WEB_MAL_HPP_
#define CATALIST_WEB_MAL_HPP_

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/entry.hpp"
#include "core/result.hpp"
#include "web/tracker.hpp"

// MyAnimeList's official API (v2): the title search and what is known about
// one title. Every request carries the program's client id, even for public
// data. It knows only its own ids, and does not say how many episodes of a
// running show have aired.
namespace catalist::mal {

inline const std::string kApiUrl = "https://api.myanimelist.net/v2";

// The request URLs, and readers for their replies.
[[nodiscard]] std::string searchUrl(mediaType type, const std::string& query);
[[nodiscard]] result<std::vector<web::match>> parseSearch(const std::string& body, mediaType type);
[[nodiscard]] std::string titleUrl(mediaType type, int malId);
[[nodiscard]] result<web::details> parseTitle(const std::string& body, mediaType type);

class tracker : public web::iTracker {
public:
	explicit tracker(web::link net) : net_(std::move(net)) {}

	[[nodiscard]] provider site() const override { return provider::kMyAnimeList; }
	[[nodiscard]] result<std::vector<web::match>> search(mediaType type,
	                                                     const std::string& query) override;
	[[nodiscard]] result<web::details> lookUp(const entry& known) override;

protected:
	void pause() const override { net_.pause(); }

private:
	// The reply to a GET with the client id; nothing when MyAnimeList cannot
	// be reached.
	[[nodiscard]] std::optional<std::string> get(const std::string& url) const;
	// Why nothing can be asked: no client id. Empty when something can.
	[[nodiscard]] std::string cannotAsk() const;

	web::link net_;
};

}  // namespace catalist::mal

#endif  // CATALIST_WEB_MAL_HPP_
