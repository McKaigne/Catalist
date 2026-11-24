#ifndef CATALIST_STORAGE_JOURNAL_HPP_
#define CATALIST_STORAGE_JOURNAL_HPP_

#include <optional>
#include <string>
#include <vector>

#include "core/entry.hpp"
#include "core/result.hpp"

namespace catalist::journal {

struct Change {
	MediaType type = MediaType::kAnime;
	int id = 0;
	std::optional<Entry> before;
	std::optional<Entry> after;
};

[[nodiscard]] std::vector<Change> Diff(const std::vector<Entry>& saved,
                                       const std::vector<Entry>& current);

struct FieldChange {
	std::string field;
	std::string before;
	std::string after;
};

[[nodiscard]] std::vector<FieldChange> ChangedFields(const Change& made);

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
[[nodiscard]] Result<std::optional<Entry>> Transition(const std::vector<Entry>& list,
                                                      const std::optional<Entry>& from,
                                                      const std::optional<Entry>& to, bool force);

}  // namespace catalist::journal

#endif  // CATALIST_STORAGE_JOURNAL_HPP_
