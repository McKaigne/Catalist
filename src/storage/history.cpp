#include <algorithm>
#include <format>

#include "storage/catalogue.hpp"
#include "storage/csv_codec.hpp"

// The undo history of the catalogue: each save with a note is a step in
// `tx`, and its changes are rows of `change`, each entry before and after
// as a csv_codec row (empty: the entry did not exist), as Catalist-TUI
// writes them too.
namespace catalist {

namespace {

std::optional<Entry> DecodeRow(const std::string& row) {
	return row.empty() ? std::nullopt : csv_codec::DecodeEntry(csv_codec::SplitFields(row));
}

std::string EncodeRow(const std::optional<Entry>& row) {
	return row ? csv_codec::EncodeEntry(*row) : std::string();
}

// The changes of one step in the order they were made; nothing when they
// cannot be read.
std::optional<std::vector<journal::Change>> ReadChanges(const store::Database& db, long long tx) {
	std::vector<journal::Change> changes;
	auto rows = db.Prepare("SELECT type, id, before, after FROM change WHERE tx = ? ORDER BY seq");
	rows.Bind(tx);
	while (rows.Step()) {
		MediaType type = rows.TextAt(0) == "manga" ? MediaType::kManga : MediaType::kAnime;
		changes.push_back({type, static_cast<int>(rows.IntAt(1)), DecodeRow(rows.TextAt(2)),
		                   DecodeRow(rows.TextAt(3))});
	}
	return rows.Ok() ? std::optional(changes) : std::nullopt;
}

bool WriteChanges(const store::Database& db, long long tx,
                  const std::vector<journal::Change>& changes) {
	auto insert = db.Prepare("INSERT INTO change VALUES (?, ?, ?, ?, ?, ?)");
	int seq = 0;
	for (const auto& made : changes) {
		insert.Bind(tx).Bind(seq++).Bind(TypeKey(made.type)).Bind(made.id);
		if (not insert.Bind(EncodeRow(made.before)).Bind(EncodeRow(made.after)).Run())
			return false;
	}
	return true;
}

// The steps that `where` selects and orders (all of them when `limit` is
// not positive); nothing when they cannot be read.
std::optional<std::vector<TxRecord>> ReadSteps(const store::Database& db, const std::string& where,
                                               int limit) {
	std::vector<TxRecord> steps;
	auto rows = db.Prepare(std::format(
	    "SELECT id, stamp, label, undone,"
	    " (SELECT COUNT(*) FROM change WHERE change.tx = tx.id) FROM tx WHERE {} LIMIT ?",
	    where));
	rows.Bind(limit > 0 ? limit : -1);
	while (rows.Step())
		steps.push_back({rows.IntAt(0), rows.TextAt(1), rows.TextAt(2), rows.IntAt(3) != 0,
		                 static_cast<int>(rows.IntAt(4))});
	return rows.Ok() ? std::optional(steps) : std::nullopt;
}

// Enters the changes as a step, forgetting the oldest beyond the last 100.
bool WriteJournal(store::Database& db, const TxNote& note,
                  const std::vector<journal::Change>& changes) {
	// New work ends what could be redone.
	if (not db.Run("DELETE FROM change WHERE tx IN (SELECT id FROM tx WHERE undone = 1);"
	               "DELETE FROM tx WHERE undone = 1;"))
		return false;
	auto insert = db.Prepare("INSERT INTO tx (stamp, label) VALUES (?, ?)");
	if (not insert.Bind(note.stamp).Bind(note.label).Run() or
	    not WriteChanges(db, db.LastInsertId(), changes))
		return false;
	return db.Run(
	    std::format("DELETE FROM tx WHERE id NOT IN (SELECT id FROM tx ORDER BY id DESC LIMIT {});"
	                "DELETE FROM change WHERE tx NOT IN (SELECT id FROM tx);",
	                Catalogue::kHistoryLength));
}

}  // namespace

bool Catalogue::SaveAll(const TxNote& note) {
	auto changes = journal::Diff(savedAnime_, animeEntries_);
	auto manga = journal::Diff(savedManga_, mangaEntries_);
	changes.insert(changes.end(), manga.begin(), manga.end());
	if (changes.empty())
		return SaveAll();
	bool done = Commit([&] { return WriteChanged() and WriteJournal(db_, note, changes); });
	if (done)
		RememberSaved();
	return done;
}

// Undo and redo.

Result<Replay> Catalogue::Undo(int count, bool force) {
	return WalkHistory(false, count, force);
}

Result<Replay> Catalogue::Redo(int count, bool force) {
	return WalkHistory(true, count, force);
}

std::string Catalogue::ReplayStep(const TxRecord& step, bool redoing, bool force) {
	auto changes = ReadChanges(db_, step.id);
	if (not changes)
		return std::format("cannot read {}: {}", StorePath().string(), db_.LastError());
	// What was done last is taken back first.
	if (not redoing)
		std::reverse(changes->begin(), changes->end());
	for (const auto& made : *changes) {
		const auto& from = redoing ? made.before : made.after;
		const auto& to = redoing ? made.after : made.before;
		auto next = journal::Transition(GetList(made.type), from, to, force);
		if (not next.has_value())
			return std::format("cannot {} '{}' ({}): {}", redoing ? "redo" : "undo", step.label,
			                   step.stamp, next.error());
		if (not next.value())
			RemoveEntry(made.type, made.id);
		else if (not UpdateEntry(*next.value()))
			RestoreEntry(*next.value());
	}
	return "";
}

Result<Replay> Catalogue::WalkHistory(bool redoing, int count, bool force) {
	auto opened = OpenStore();
	if (not opened.has_value())
		return std::unexpected(opened.error());
	auto steps = ReadSteps(
	    db_, redoing ? "undone = 1 ORDER BY id ASC" : "undone = 0 ORDER BY id DESC", count);
	if (not steps)
		return std::unexpected(
		    std::format("cannot read {}: {}", StorePath().string(), db_.LastError()));
	if (steps->empty())
		return {};

	// The lists as they are, to go back to when a step cannot be taken.
	std::vector<Entry> anime = animeEntries_;
	std::vector<Entry> manga = mangaEntries_;
	std::string problem;
	for (size_t i = 0; i < steps->size() and problem.empty(); ++i)
		problem = ReplayStep((*steps)[i], redoing, force);

	auto mark = [&] {
		auto set = db_.Prepare("UPDATE tx SET undone = ? WHERE id = ?");
		return std::all_of(steps->begin(), steps->end(), [&](const TxRecord& step) {
			return set.Bind(redoing ? 0 : 1).Bind(step.id).Run();
		});
	};
	if (problem.empty() and not Commit([&] { return WriteChanged() and mark(); }))
		problem = std::format("could not save the catalogue to {}", data_dir().string());
	if (not problem.empty()) {
		animeEntries_ = std::move(anime);
		mangaEntries_ = std::move(manga);
		return std::unexpected(problem);
	}

	Replay done{*steps, journal::Diff(anime, animeEntries_)};
	auto more = journal::Diff(manga, mangaEntries_);
	done.changes.insert(done.changes.end(), more.begin(), more.end());
	RememberSaved();
	return done;
}

Result<std::vector<TxRecord>> Catalogue::History(int limit) {
	if (not db_.IsOpen())
		return {};
	auto steps = ReadSteps(db_, "1 ORDER BY id DESC", limit);
	if (not steps)
		return std::unexpected(
		    std::format("cannot read {}: {}", StorePath().string(), db_.LastError()));
	return std::move(*steps);
}

}  // namespace catalist
