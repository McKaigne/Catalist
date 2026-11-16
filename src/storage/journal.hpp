#ifndef CATALIST_STORAGE_JOURNAL_HPP_
#define CATALIST_STORAGE_JOURNAL_HPP_

#include <optional>
#include <string>
#include <vector>

#include "core/entry.hpp"
#include "core/result.hpp"

// The undo history as plain data: what one command changed, how two sets of
// changes fold into one, and what taking a change back does to a list. Pure
// functions; the catalogue stores the records and applies the outcome.
//
// Only what the person sets counts as a change: the title, the status, the
// score, the progress, the total, the volumes, the rewatch count and the
// two dates. What a web check fills in (ids, released episodes, the volume
// total) is never recorded and never undone.
namespace catalist::journal {

// One entry before and after a command. No `before`: the command added it.
// No `after`: the command removed it.
struct change {
	mediaType type = mediaType::kAnime;
	int id = 0;
	std::optional<entry> before;
	std::optional<entry> after;
};

// The entries of one list that differ between `saved` and `current`, by
// their id: those added or changed in the order of `current`, then those
// removed.
[[nodiscard]] std::vector<change> diff(const std::vector<entry>& saved,
                                       const std::vector<entry>& current);

// One field of a change as text, the way a command reports it.
struct fieldChange {
	std::string field;
	std::string before;
	std::string after;
};

// The fields a change changed: progress, status, score, total, volumes,
// rewatched, started, finished and title, in that order. An added entry is
// the one field `added` (from "-" to its status), a removed one `removed`
// (from its status to "-"). An absent date is "-".
[[nodiscard]] std::vector<fieldChange> changedFields(const change& made);

// What stands in `list` once an entry goes from `from` back (or on) to `to`:
// the entry to put there, or nothing when it is to be removed. Undo passes a
// change's `after` and `before`; redo passes them the other way round.
//
// The list must still hold what `from` says in every field the change
// changed; if not, the result is the reason, naming the entry, and `force`
// skips that check. Only those fields are set (and the day of the last
// change); everything else keeps its present value. An entry that comes
// back does so under its old id, which fails, forced or not, when that id
// or one of its site ids belongs to another entry by now.
[[nodiscard]] result<std::optional<entry>> transition(const std::vector<entry>& list,
                                                      const std::optional<entry>& from,
                                                      const std::optional<entry>& to, bool force);

}  // namespace catalist::journal

#endif  // CATALIST_STORAGE_JOURNAL_HPP_
