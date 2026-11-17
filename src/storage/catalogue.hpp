#ifndef CATALIST_STORAGE_CATALOGUE_HPP_
#define CATALIST_STORAGE_CATALOGUE_HPP_

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/date.hpp"
#include "core/entry.hpp"
#include "core/result.hpp"
#include "storage/journal.hpp"
#include "storage/store.hpp"

namespace catalist {

// The environment variables that decide where the data is kept; empty
// when unset.
struct DataDirEnv {
	std::string catalist_data_dir;
	std::string xdg_data_home;
	std::string home;
};

// Where the lists are kept: CATALIST_DATA_DIR; else $XDG_DATA_HOME/catalist
// when that is an absolute path; else ~/.local/share/catalist.
[[nodiscard]] std::filesystem::path DataDirFor(const DataDirEnv& env);

// How a save is entered in the undo history.
struct TxNote {
	// What the step is called there: the command as it was typed.
	std::string label;
	// The local date and time.
	std::string stamp;
};

// A title a site is still to be told of: what the lists now hold of it, or,
// for a title removed from both, its id on that site.
struct Pending {
	Provider site = Provider::kAniList;
	MediaType type = MediaType::kAnime;
	int id = 0;
	bool removed = false;
	int site_id = 0;
	// What to send (site_field bits): what changed since the site was last
	// told.
	unsigned fields = site_field::kAll;
	// The tries that failed, and when to try again (seconds since 1970; 0
	// is at once).
	int attempts = 0;
	long long next_try = 0;
};

// One step of the undo history: what one command changed.
struct TxRecord {
	long long id = 0;
	std::string stamp;
	std::string label;
	bool undone = false;
	// How many entries it changed.
	int changes = 0;
};

// What an undo or a redo did: the steps in the order they were taken, and
// what became of the entries.
struct Replay {
	std::vector<TxRecord> steps;
	std::vector<journal::Change> changes;
};

// The anime and manga lists in memory, and the SQLite file that keeps them
// (`catalist.db` in the data directory). Changes are made in memory and
// written by SaveAll, which writes only the rows that differ from what was
// last read or written. The same file keeps the undo history: the last 100
// saves that were given a note.
class Catalogue {
public:
	static constexpr int kHistoryLength = 100;

	explicit Catalogue(std::filesystem::path data_dir);

	// Adds a title under a fresh id and returns it. A title that shares its
	// AniList, MyAnimeList or Kitsu id with a stored one is merged into that entry
	// instead (see MergeImported), so importing a list twice adds nothing.
	int AddEntry(Entry added);
	[[nodiscard]] const std::vector<Entry>& GetEntries(MediaType type) const;
	[[nodiscard]] std::optional<Entry> GetById(MediaType type, int id) const;
	// The entry with any of the ids (0 matches nothing).
	[[nodiscard]] std::optional<Entry> FindByIds(MediaType type, int anilist_id, int mal_id,
	                                             int kitsu_id = 0) const;

	// The site whose ids are shown, kept beside the lists (AniList when
	// nothing is stored). LoadAll reads it; SaveSettings and SaveAll write
	// it.
	[[nodiscard]] Provider provider() const;
	void set_provider(Provider site);
	[[nodiscard]] bool SaveSettings();

	bool UpdateEntry(const Entry& changed);
	bool RemoveEntry(MediaType type, int id);
	// Puts a removed entry back under the id it had; false when another
	// entry has that id by now.
	bool RestoreEntry(const Entry& back);

	[[nodiscard]] std::filesystem::path data_dir() const;
	[[nodiscard]] std::filesystem::path StorePath() const;

	// Makes this program the only writer from now until the next save (or
	// until the catalogue goes away): what LoadAll reads next cannot change
	// under a command before it saves. Fails when another program holds the
	// catalogue for longer than a few seconds.
	[[nodiscard]] Done BeginWrite();

	// Reads the lists and the provider. The value says whether there was
	// anything stored; with nothing stored, nothing is created until the
	// first save. Fails, naming the file, when it cannot be read.
	[[nodiscard]] Result<bool> LoadAll();
	// Reads the lists as LoadAll does, but only looks: nothing is created, no
	// write is waited for, and the file is closed again. False
	// when there is no file or it cannot be read just now. For the shell's
	// completion, which must never get in the way.
	[[nodiscard]] bool Peek();

	// Writes the rows that changed since the last load or save, and the
	// provider when it changed, as one unit: all of it or nothing. Not
	// entered in the undo history: for what the web filled in.
	[[nodiscard]] bool SaveAll();
	// The same, entered in the undo history as one step when anything the
	// person sets has changed (see journal.hpp). Steps that were undone are
	// forgotten then: new work ends what could be redone.
	[[nodiscard]] bool SaveAll(const TxNote& note);

	// Takes back the newest `count` steps that are not undone, newest first,
	// or makes the oldest `count` undone steps again, and saves. All of it or
	// nothing: when an entry is no longer as a step left it, the lists stay
	// as they are and the result names the step and the entry. `force` takes
	// the steps back all the same. Fewer steps than `count` is not an error;
	// the result says which were taken.
	[[nodiscard]] Result<Replay> Undo(int count, bool force);
	[[nodiscard]] Result<Replay> Redo(int count, bool force);
	// The steps of the history, newest first; all of them when `limit` is 0.
	// Empty until LoadAll has found something stored.
	[[nodiscard]] Result<std::vector<TxRecord>> History(int limit);

	// ---- What the sites are still to be told ----

	// The titles waiting, oldest first; none when nothing waits or the file
	// cannot be read.
	[[nodiscard]] std::vector<Pending> Outbox();
	// Adds them; one already waiting gets the fields of both (MergeFields)
	// and is tried again at once.
	[[nodiscard]] bool Queue(const std::vector<Pending>& more);
	// Keeps one waiting after a try that failed: how many have, and when to
	// try again.
	[[nodiscard]] bool Deferred(const Pending& waiting, int attempts, long long next_try);
	// Takes one off: it was told, or it cannot be.
	[[nodiscard]] bool Sent(const Pending& done);

	// Whether the person hides the title on the site from others (track
	// --private), as said while it had `site_id` there; nothing while they
	// have not said.
	[[nodiscard]] std::optional<bool> HiddenOn(Provider site, MediaType type, int id, int site_id);
	[[nodiscard]] bool SetHidden(Provider site, MediaType type, int id, int site_id, bool hidden);

private:
	std::vector<Entry>& GetList(MediaType type);
	const std::vector<Entry>& GetList(MediaType type) const;
	const std::vector<Entry>& GetSaved(MediaType type) const;

	[[nodiscard]] Done OpenStore();
	[[nodiscard]] Done SetUpStore();
	[[nodiscard]] bool ReadStore();
	[[nodiscard]] bool WriteRows();
	[[nodiscard]] bool WriteSiteIds();
	[[nodiscard]] bool WriteSettings();
	// The rows, and the settings when they changed.
	[[nodiscard]] bool WriteChanged();
	[[nodiscard]] Result<Replay> WalkHistory(bool redoing, int count, bool force);
	// Applies one step to the lists in memory; the reason when it cannot.
	[[nodiscard]] std::string ReplayStep(const TxRecord& step, bool redoing, bool force);
	// Runs `work` inside the write that BeginWrite opened, or one of its
	// own, and commits it.
	[[nodiscard]] bool Commit(const std::function<bool()>& work);
	void RememberSaved();

	std::filesystem::path dataDir_;
	std::vector<Entry> animeEntries_;
	std::vector<Entry> mangaEntries_;
	Provider provider_ = Provider::kAniList;

	// What the store holds, as far as this program knows: the lists and
	// settings as last read or written.
	std::vector<Entry> savedAnime_;
	std::vector<Entry> savedManga_;
	Provider savedProvider_ = Provider::kAniList;

	// The write is declared after the database: it must end first.
	store::Database db_;
	std::optional<store::Transaction> write_;
};

}  // namespace catalist

#endif  // CATALIST_STORAGE_CATALOGUE_HPP_
