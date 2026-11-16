#include <algorithm>
#include <format>

#include "storage/catalogue.hpp"
#include "storage/csvCodec.hpp"

// The undo history of the catalogue: each save with a note is a step in
// `tx`, and its changes are rows of `change`, each entry before and after
// as a csvCodec row (empty: the entry did not exist), as Catalist-TUI
// writes them too.
namespace catalist {

namespace {

std::optional<entry> decodeRow(const std::string& row) {
	return row.empty() ? std::nullopt : csvCodec::decodeEntry(csvCodec::splitFields(row));
}

std::string encodeRow(const std::optional<entry>& row) {
	return row ? csvCodec::encodeEntry(*row) : std::string();
}

// The changes of one step in the order they were made; nothing when they
// cannot be read.
std::optional<std::vector<journal::change>> readChanges(const store::database& db, long long tx) {
	std::vector<journal::change> changes;
	auto rows = db.prepare("SELECT type, id, before, after FROM change WHERE tx = ? ORDER BY seq");
	rows.bind(tx);
	while (rows.step()) {
		mediaType type = rows.textAt(0) == "manga" ? mediaType::kManga : mediaType::kAnime;
		changes.push_back({type, static_cast<int>(rows.intAt(1)), decodeRow(rows.textAt(2)),
		                   decodeRow(rows.textAt(3))});
	}
	return rows.ok() ? std::optional(changes) : std::nullopt;
}

bool writeChanges(const store::database& db, long long tx,
                  const std::vector<journal::change>& changes) {
	auto insert = db.prepare("INSERT INTO change VALUES (?, ?, ?, ?, ?, ?)");
	int seq = 0;
	for (const auto& made : changes) {
		insert.bind(tx).bind(seq++).bind(typeKey(made.type)).bind(made.id);
		if (not insert.bind(encodeRow(made.before)).bind(encodeRow(made.after)).run())
			return false;
	}
	return true;
}

// The steps that `where` selects and orders (all of them when `limit` is
// not positive); nothing when they cannot be read.
std::optional<std::vector<txRecord>> readSteps(const store::database& db, const std::string& where,
                                               int limit) {
	std::vector<txRecord> steps;
	auto rows = db.prepare(std::format(
	    "SELECT id, stamp, label, undone,"
	    " (SELECT COUNT(*) FROM change WHERE change.tx = tx.id) FROM tx WHERE {} LIMIT ?",
	    where));
	rows.bind(limit > 0 ? limit : -1);
	while (rows.step())
		steps.push_back({rows.intAt(0), rows.textAt(1), rows.textAt(2), rows.intAt(3) != 0,
		                 static_cast<int>(rows.intAt(4))});
	return rows.ok() ? std::optional(steps) : std::nullopt;
}

// Enters the changes as a step, forgetting the oldest beyond the last 100.
bool writeJournal(store::database& db, const txNote& note,
                  const std::vector<journal::change>& changes) {
	// New work ends what could be redone.
	if (not db.run("DELETE FROM change WHERE tx IN (SELECT id FROM tx WHERE undone = 1);"
	               "DELETE FROM tx WHERE undone = 1;"))
		return false;
	auto insert = db.prepare("INSERT INTO tx (stamp, label) VALUES (?, ?)");
	if (not insert.bind(note.stamp).bind(note.label).run() or
	    not writeChanges(db, db.lastInsertId(), changes))
		return false;
	return db.run(
	    std::format("DELETE FROM tx WHERE id NOT IN (SELECT id FROM tx ORDER BY id DESC LIMIT {});"
	                "DELETE FROM change WHERE tx NOT IN (SELECT id FROM tx);",
	                catalogue::kHistoryLength));
}

}  // namespace

bool catalogue::saveAll(const txNote& note) {
	auto changes = journal::diff(savedAnime_, animeEntries_);
	auto manga = journal::diff(savedManga_, mangaEntries_);
	changes.insert(changes.end(), manga.begin(), manga.end());
	if (changes.empty())
		return saveAll();
	bool done = commit([&] { return writeChanged() and writeJournal(db_, note, changes); });
	if (done)
		rememberSaved();
	return done;
}

// Undo and redo.

result<replay> catalogue::undo(int count, bool force) {
	return walkHistory(false, count, force);
}

result<replay> catalogue::redo(int count, bool force) {
	return walkHistory(true, count, force);
}

std::string catalogue::replayStep(const txRecord& step, bool redoing, bool force) {
	auto changes = readChanges(db_, step.id);
	if (not changes)
		return std::format("cannot read {}: {}", getStorePath().string(), db_.lastError());
	// What was done last is taken back first.
	if (not redoing)
		std::reverse(changes->begin(), changes->end());
	for (const auto& made : *changes) {
		const auto& from = redoing ? made.before : made.after;
		const auto& to = redoing ? made.after : made.before;
		auto next = journal::transition(getList(made.type), from, to, force);
		if (not next.has_value())
			return std::format("cannot {} '{}' ({}): {}", redoing ? "redo" : "undo", step.label,
			                   step.stamp, next.error());
		if (not next.value())
			removeEntry(made.type, made.id);
		else if (not updateEntry(*next.value()))
			restoreEntry(*next.value());
	}
	return "";
}

result<replay> catalogue::walkHistory(bool redoing, int count, bool force) {
	auto opened = openStore();
	if (not opened.has_value())
		return std::unexpected(opened.error());
	auto steps = readSteps(
	    db_, redoing ? "undone = 1 ORDER BY id ASC" : "undone = 0 ORDER BY id DESC", count);
	if (not steps)
		return std::unexpected(
		    std::format("cannot read {}: {}", getStorePath().string(), db_.lastError()));
	if (steps->empty())
		return {};

	// The lists as they are, to go back to when a step cannot be taken.
	std::vector<entry> anime = animeEntries_;
	std::vector<entry> manga = mangaEntries_;
	std::string problem;
	for (size_t i = 0; i < steps->size() and problem.empty(); ++i)
		problem = replayStep((*steps)[i], redoing, force);

	auto mark = [&] {
		auto set = db_.prepare("UPDATE tx SET undone = ? WHERE id = ?");
		return std::all_of(steps->begin(), steps->end(), [&](const txRecord& step) {
			return set.bind(redoing ? 0 : 1).bind(step.id).run();
		});
	};
	if (problem.empty() and not commit([&] { return writeChanged() and mark(); }))
		problem = std::format("could not save the catalogue to {}", getDataDir().string());
	if (not problem.empty()) {
		animeEntries_ = std::move(anime);
		mangaEntries_ = std::move(manga);
		return std::unexpected(problem);
	}

	replay done{*steps, journal::diff(anime, animeEntries_)};
	auto more = journal::diff(manga, mangaEntries_);
	done.changes.insert(done.changes.end(), more.begin(), more.end());
	rememberSaved();
	return done;
}

result<std::vector<txRecord>> catalogue::history(int limit) {
	if (not db_.isOpen())
		return {};
	auto steps = readSteps(db_, "1 ORDER BY id DESC", limit);
	if (not steps)
		return std::unexpected(
		    std::format("cannot read {}: {}", getStorePath().string(), db_.lastError()));
	return std::move(*steps);
}

}  // namespace catalist
