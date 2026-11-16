#ifndef CATALIST_CORE_ENTRY_HPP_
#define CATALIST_CORE_ENTRY_HPP_

#include <optional>
#include <string>

#include "core/date.hpp"

namespace catalist {

enum class mediaType { kAnime, kManga };

// The site whose ids the program shows and accepts, and which a future sync
// would keep the catalogue in step with. Only one is chosen at a time.
enum class provider { kAniList, kMyAnimeList };

enum class watchStatus { kWatching, kCompleted, kOnHold, kDropped, kPlanToWatch };

struct entry {
	// Numbers the title within its list: the 12 of the ref a:12.
	int id = 0;
	// The title on each site; 0 while not known. AniList knows both, so a
	// lookup fills in whichever is missing.
	int anilistId = 0;
	int malId = 0;
	std::string title;
	mediaType type = mediaType::kAnime;
	watchStatus status = watchStatus::kWatching;
	int score = 0;
	int progress = 0;
	int totalUnits = 0;
	int volumes = 0;
	int totalVolumes = 0;
	int rewatched = 0;
	std::optional<isoDate> startDate;
	std::optional<isoDate> finishDate;
	std::optional<isoDate> updatedAt;
	// Episodes or chapters out so far, from the last web check (0: unknown),
	// and when that check was.
	int released = 0;
	std::optional<isoDate> checkedAt;
};

std::string statusToString(watchStatus status, mediaType type = mediaType::kAnime);
// The one word a status is stored and printed for scripts as: watching,
// completed, on-hold, dropped or planned. parseStatus reads it back.
[[nodiscard]] std::string statusKey(watchStatus status);
// Accepts the display names and MyAnimeList's 1-5 codes, in any case;
// absent for anything else.
[[nodiscard]] std::optional<watchStatus> parseStatus(const std::string& str);

std::string typeToString(mediaType type);
// "anime" or "manga".
[[nodiscard]] std::string typeKey(mediaType type);

// "AniList" or "MyAnimeList".
[[nodiscard]] std::string providerName(provider site);
// "anilist", "al", "myanimelist" or "mal", in any case.
[[nodiscard]] std::optional<provider> parseProvider(const std::string& name);
// The title's id on the site, 0 when not known, and its page there (empty
// then).
[[nodiscard]] int idOn(const entry& shown, provider site);
[[nodiscard]] std::string pageOn(const entry& shown, provider site);

// The display and file form of an optional date: ISO text, or empty.
[[nodiscard]] std::string formatDate(const std::optional<isoDate>& date);

// The one date the list shows for an entry: when it was finished if it is
// completed, else when it was started, else when it was last changed.
[[nodiscard]] std::optional<isoDate> listDate(const entry& shown);

// Released episodes or chapters not yet watched or read, for titles being
// followed (watching or on hold); 0 for everything else.
[[nodiscard]] int newReleases(const entry& shown);

}  // namespace catalist

// Pure watchlist rules. Each takes the entry by value and returns the updated
// copy; `today` stamps updatedAt and any start or finish date the rule fills
// in. A change that changes nothing returns the entry exactly as it was.
namespace catalist {

// Moves progress by delta, never below zero nor past a known total. Moving
// forward starts the entry (start date, and resumes it from planned, on-hold
// or dropped); reaching the total completes it with a finish date. Moving
// back below the total reopens a completed entry.
[[nodiscard]] entry stepProgress(entry target, int delta, const isoDate& today);

// Sets the status, keeping the start and finish dates and the progress
// consistent with it. Setting the status it already has changes nothing.
[[nodiscard]] entry setStatus(entry target, watchStatus status, const isoDate& today);

// Sets the score, kept within 0..10.
[[nodiscard]] entry setScore(entry target, int score, const isoDate& today);

// The numbers of an entry the user can set directly.
enum class countField { kProgress, kTotalUnits, kVolumes, kRewatched };

[[nodiscard]] int countOf(const entry& target, countField field);

// Sets one count, never below zero. Progress follows the rules of
// stepProgress; a total below the progress lowers the progress to it; volumes
// stay within a known volume total.
[[nodiscard]] entry setCount(entry target, countField field, int value, const isoDate& today);

// Renames the entry.
[[nodiscard]] entry setTitle(entry target, const std::string& title, const isoDate& today);

// Sets the day the entry was started or finished, or clears it. The status
// is left as it is: these correct a date, they do not start or finish.
[[nodiscard]] entry setStartDate(entry target, std::optional<isoDate> day, const isoDate& today);
[[nodiscard]] entry setFinishDate(entry target, std::optional<isoDate> day, const isoDate& today);

// Folds an imported row into an entry the user already has. Totals and the
// ids are refreshed, but the user's own score, progress and dates
// win: imported values only fill fields the user never set.
[[nodiscard]] entry mergeImported(entry existing, const entry& incoming);

// What a web check found out about a title; 0 means it did not say.
struct webFacts {
	int totalUnits = 0;
	int totalVolumes = 0;
	int released = 0;
	int anilistId = 0;
	int malId = 0;
};

// Takes the totals and the released count from a web check, keeping the
// known values where the check had none, fills in an id the entry lacked,
// and stamps checkedAt. The user's
// own fields, updatedAt included, are left alone.
[[nodiscard]] entry applyWebFacts(entry target, const webFacts& facts, const isoDate& today);

}  // namespace catalist

#endif  // CATALIST_CORE_ENTRY_HPP_
