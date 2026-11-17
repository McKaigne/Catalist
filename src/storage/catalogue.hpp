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
struct dataDirEnv {
	std::string catalistDataDir;
	std::string xdgDataHome;
	std::string home;
};

// Where the lists are kept: CATALIST_DATA_DIR; else $XDG_DATA_HOME/catalist
// when that is an absolute path; else ~/.local/share/catalist.
[[nodiscard]] std::filesystem::path dataDirFor(const dataDirEnv& env);

// How a save is entered in the undo history.
struct txNote {
	// What the step is called there: the command as it was typed.
	std::string label;
	// The local date and time.
	std::string stamp;
};

// A title a site is still to be told of: what the lists now hold of it, or,
// for a title removed from both, its id on that site.
struct pending {
	provider site = provider::kAniList;
	mediaType type = mediaType::kAnime;
	int id = 0;
	bool removed = false;
	int siteId = 0;
	// What to send (siteField bits): what changed since the site was last
	// told.
	unsigned fields = siteField::kAll;
	// The tries that failed, and when to try again (seconds since 1970; 0
	// is at once).
	int attempts = 0;
	long long nextTry = 0;
};

// One step of the undo history: what one command changed.
struct txRecord {
	long long id = 0;
	std::string stamp;
	std::string label;
	bool undone = false;
	// How many entries it changed.
	int changes = 0;
};

// What an undo or a redo did: the steps in the order they were taken, and
// what became of the entries.
struct replay {
	std::vector<txRecord> steps;
	std::vector<journal::change> changes;
};

// The anime and manga lists in memory, and the SQLite file that keeps them
// (`catalist.db` in the data directory). Changes are made in memory and
// written by saveAll, which writes only the rows that differ from what was
// last read or written. The same file keeps the undo history: the last 100
// saves that were given a note.
class catalogue {
public:
	static constexpr int kHistoryLength = 100;

	explicit catalogue(std::filesystem::path dataDir);

	// Adds a title under a fresh id and returns it. A title that shares its
	// AniList, MyAnimeList or Kitsu id with a stored one is merged into that entry
	// instead (see mergeImported), so importing a list twice adds nothing.
	int addEntry(entry added);
	[[nodiscard]] const std::vector<entry>& getEntries(mediaType type) const;
	[[nodiscard]] std::optional<entry> getById(mediaType type, int id) const;
	// The entry with any of the ids (0 matches nothing).
	[[nodiscard]] std::optional<entry> findByIds(mediaType type, int anilistId, int malId,
	                                             int kitsuId = 0) const;

	// The site whose ids are shown, kept beside the lists (AniList when
	// nothing is stored). loadAll reads it; saveSettings and saveAll write
	// it.
	[[nodiscard]] provider getProvider() const;
	void setProvider(provider site);
	[[nodiscard]] bool saveSettings();

	bool updateEntry(const entry& changed);
	bool removeEntry(mediaType type, int id);
	// Puts a removed entry back under the id it had; false when another
	// entry has that id by now.
	bool restoreEntry(const entry& back);

	[[nodiscard]] std::filesystem::path getDataDir() const;
	[[nodiscard]] std::filesystem::path getStorePath() const;

	// Makes this program the only writer from now until the next save (or
	// until the catalogue goes away): what loadAll reads next cannot change
	// under a command before it saves. Fails when another program holds the
	// catalogue for longer than a few seconds.
	[[nodiscard]] done beginWrite();

	// Reads the lists and the provider. The value says whether there was
	// anything stored; with nothing stored, nothing is created until the
	// first save. Fails, naming the file, when it cannot be read.
	[[nodiscard]] result<bool> loadAll();
	// Reads the lists as loadAll does, but only looks: nothing is created, no
	// write is waited for, and the file is closed again. False
	// when there is no file or it cannot be read just now. For the shell's
	// completion, which must never get in the way.
	[[nodiscard]] bool peek();

	// Writes the rows that changed since the last load or save, and the
	// provider when it changed, as one unit: all of it or nothing. Not
	// entered in the undo history: for what the web filled in.
	[[nodiscard]] bool saveAll();
	// The same, entered in the undo history as one step when anything the
	// person sets has changed (see journal.hpp). Steps that were undone are
	// forgotten then: new work ends what could be redone.
	[[nodiscard]] bool saveAll(const txNote& note);

	// Takes back the newest `count` steps that are not undone, newest first,
	// or makes the oldest `count` undone steps again, and saves. All of it or
	// nothing: when an entry is no longer as a step left it, the lists stay
	// as they are and the result names the step and the entry. `force` takes
	// the steps back all the same. Fewer steps than `count` is not an error;
	// the result says which were taken.
	[[nodiscard]] result<replay> undo(int count, bool force);
	[[nodiscard]] result<replay> redo(int count, bool force);
	// The steps of the history, newest first; all of them when `limit` is 0.
	// Empty until loadAll has found something stored.
	[[nodiscard]] result<std::vector<txRecord>> history(int limit);

	// ---- What the sites are still to be told ----

	// The titles waiting, oldest first; none when nothing waits or the file
	// cannot be read.
	[[nodiscard]] std::vector<pending> outbox();
	// Adds them; one already waiting gets the fields of both (mergeFields)
	// and is tried again at once.
	[[nodiscard]] bool queue(const std::vector<pending>& more);
	// Keeps one waiting after a try that failed: how many have, and when to
	// try again.
	[[nodiscard]] bool deferred(const pending& waiting, int attempts, long long nextTry);
	// Takes one off: it was told, or it cannot be.
	[[nodiscard]] bool sent(const pending& done);

private:
	std::vector<entry>& getList(mediaType type);
	const std::vector<entry>& getList(mediaType type) const;
	const std::vector<entry>& getSaved(mediaType type) const;

	[[nodiscard]] done openStore();
	[[nodiscard]] done setUpStore();
	[[nodiscard]] bool readStore();
	[[nodiscard]] bool writeRows();
	[[nodiscard]] bool writeSiteIds();
	[[nodiscard]] bool writeSettings();
	// The rows, and the settings when they changed.
	[[nodiscard]] bool writeChanged();
	[[nodiscard]] result<replay> walkHistory(bool redoing, int count, bool force);
	// Applies one step to the lists in memory; the reason when it cannot.
	[[nodiscard]] std::string replayStep(const txRecord& step, bool redoing, bool force);
	// Runs `work` inside the write that beginWrite opened, or one of its
	// own, and commits it.
	[[nodiscard]] bool commit(const std::function<bool()>& work);
	void rememberSaved();

	std::filesystem::path dataDir_;
	std::vector<entry> animeEntries_;
	std::vector<entry> mangaEntries_;
	provider provider_ = provider::kAniList;

	// What the store holds, as far as this program knows: the lists and
	// settings as last read or written.
	std::vector<entry> savedAnime_;
	std::vector<entry> savedManga_;
	provider savedProvider_ = provider::kAniList;

	// The write is declared after the database: it must end first.
	store::database db_;
	std::optional<store::transaction> write_;
};

}  // namespace catalist

#endif  // CATALIST_STORAGE_CATALOGUE_HPP_
