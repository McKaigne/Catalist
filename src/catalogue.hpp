#ifndef CATALIST_CATALOGUE_HPP_
#define CATALIST_CATALOGUE_HPP_

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "date.hpp"
#include "entry.hpp"
#include "journal.hpp"
#include "result.hpp"
#include "store.hpp"

// RFC 4180 encoding and decoding of catalogue rows: the files Catalist 0.3
// and earlier kept the lists in, which are still read once. Pure text in,
// records out.
namespace catalist::csvCodec {

[[nodiscard]] std::string catalogFileName(mediaType type);

[[nodiscard]] std::string escapeField(const std::string& field);

// Splits text into records, joining quoted fields that span lines. Blank
// lines and '#' comment lines between records are skipped; CRLF is accepted.
[[nodiscard]] std::vector<std::string> splitRecords(std::string_view text);
[[nodiscard]] std::vector<std::string> splitFields(std::string_view record);

[[nodiscard]] std::optional<isoDate> decodeDate(const std::string& field);

// The 17 columns of encodeEntries (or their first 14 or 16, written before
// the web checks and AniList ids), or the 16 that Catalist 0.2.1 to 0.3.0
// wrote (their notes are dropped). Columns after them are
// ignored; a row without the type column where either layout has it is in an
// older layout and is not read.
[[nodiscard]] std::optional<entry> decodeEntry(const std::vector<std::string>& fields,
                                               mediaType type);
// Fails on the first row decodeEntry cannot read, so a file is never loaded
// half-read.
[[nodiscard]] result<std::vector<entry>> decodeEntries(std::string_view text, mediaType type);
[[nodiscard]] std::string encodeEntries(const std::vector<entry>& entries);
// One row of encodeEntries, without the line end.
[[nodiscard]] std::string encodeEntry(const entry& row);

}  // namespace catalist::csvCodec

namespace catalist {

enum class sortKey {
	kNew,  // most new releases first, then the latest change
	kUpdated,
	kScore,
	kStatus,
	kTitle,
	kProgress,
	kDate
};

// The environment variables that decide where the data is kept; empty
// when unset.
struct dataDirEnv {
	std::string catalistDataDir;
	std::string xdgDataHome;
	std::string xdgConfigHome;
	std::string home;
};

// How a save is entered in the undo history.
struct txNote {
	// What the step is called there: the command as it was typed.
	std::string label;
	// The local date and time.
	std::string stamp;
	// Fold the save into the newest step when that one has the same label:
	// CATALIST_TX names a run of commands that one undo takes back.
	bool group = false;
};

// One step of the undo history: what one command (or one group) changed.
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

	// `oldDir` is where Catalist 0.3 and earlier kept their CSV files; it is
	// looked in once, when the data directory holds nothing yet.
	explicit catalogue(std::filesystem::path dataDir, std::filesystem::path oldDir = {});

	// Adds a title under a fresh id and returns it. A title that shares its
	// AniList or MyAnimeList id with a stored one is merged into that entry
	// instead (see mergeImported), so importing a list twice adds nothing.
	int addEntry(entry added);
	[[nodiscard]] const std::vector<entry>& getEntries(mediaType type) const;
	[[nodiscard]] std::optional<entry> getById(mediaType type, int id) const;
	// The entry with either id (0 matches nothing).
	[[nodiscard]] std::optional<entry> findByIds(mediaType type, int anilistId, int malId) const;

	// The site whose ids are shown, kept beside the lists (AniList when
	// nothing is stored). loadAll reads it; saveSettings and saveAll write
	// it.
	[[nodiscard]] provider getProvider() const;
	void setProvider(provider site);
	// The day missing ids were last looked up, so it happens once a day.
	[[nodiscard]] std::optional<isoDate> getLinkedOn() const;
	void setLinkedOn(std::optional<isoDate> day);
	[[nodiscard]] bool saveSettings();

	// Fills in the ids found for titles of one type, matching on either id;
	// returns how many entries gained one.
	int fillIds(mediaType type, const std::vector<std::pair<int, int>>& anilistAndMal);
	bool updateEntry(const entry& changed);
	bool removeEntry(mediaType type, int id);
	// Puts a removed entry back under the id it had; false when another
	// entry has that id by now.
	bool restoreEntry(const entry& back);

	// Each key has a natural order: new releases, last change, score, progress
	// and date from high to low, title and status from first to last.
	// `reverse` flips it.
	static void sortEntries(std::vector<entry>& entries, sortKey key, bool reverse);

	[[nodiscard]] size_t countByStatus(mediaType type, watchStatus status) const;
	[[nodiscard]] size_t countByScore(mediaType type, int score) const;
	// Over rated entries only; 0 when nothing is rated.
	[[nodiscard]] double averageScore(mediaType type) const;
	[[nodiscard]] int totalProgress(mediaType type) const;
	[[nodiscard]] int totalVolumes(mediaType type) const;
	[[nodiscard]] int totalRewatched(mediaType type) const;
	// An estimate: 24 minutes an episode, 5 minutes a chapter.
	[[nodiscard]] double daysSpent(mediaType type) const;

	// CATALIST_DATA_DIR; else $XDG_DATA_HOME/catalist when that is an
	// absolute path; else ~/.local/share/catalist.
	[[nodiscard]] static std::filesystem::path getDefaultDataDir(const dataDirEnv& env);
	// Where Catalist 0.3 and earlier kept the lists: $XDG_CONFIG_HOME/catalist
	// or ~/.config/catalist. Empty when CATALIST_DATA_DIR is set, which names
	// the one directory to use.
	[[nodiscard]] static std::filesystem::path getOldDataDir(const dataDirEnv& env);
	[[nodiscard]] std::filesystem::path getDataDir() const;
	[[nodiscard]] std::filesystem::path getStorePath() const;

	// Makes this program the only writer from now until the next save (or
	// until the catalogue goes away): what loadAll reads next cannot change
	// under a command before it saves. Fails when another program holds the
	// catalogue for longer than a few seconds.
	[[nodiscard]] result<bool> beginWrite();

	// Reads the lists and the provider. The value says whether there was
	// anything stored; with nothing stored, nothing is created until the
	// first save. Fails, naming the file, when it cannot be read.
	//
	// The first time (no `catalist.db` yet), the CSV files of Catalist 0.3
	// and earlier are imported, from the data directory or else from the
	// old one; they are never changed. importedFrom then names the
	// directory they were read from.
	[[nodiscard]] result<bool> loadAll();
	[[nodiscard]] const std::filesystem::path& importedFrom() const { return importedFrom_; }

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

private:
	std::vector<entry>& getList(mediaType type);
	const std::vector<entry>& getList(mediaType type) const;
	const std::vector<entry>& getSaved(mediaType type) const;

	[[nodiscard]] result<bool> openStore();
	[[nodiscard]] result<bool> setUpStore();
	[[nodiscard]] std::filesystem::path importSource() const;
	[[nodiscard]] result<bool> importOldFiles(const std::filesystem::path& from);
	[[nodiscard]] bool readStore();
	[[nodiscard]] bool writeRows();
	[[nodiscard]] bool writeSettings();
	// The rows, and the settings when they changed.
	[[nodiscard]] bool writeChanged();
	[[nodiscard]] result<replay> walkHistory(bool redoing, int count, bool force);
	// Applies one step to the lists in memory; the reason when it cannot.
	[[nodiscard]] std::string replayStep(const txRecord& step, bool redoing, bool force);
	// Runs `work` inside the write that beginWrite opened, or one of its
	// own, and commits it.
	template <typename writer>
	[[nodiscard]] bool commit(writer work);
	void rememberSaved();

	std::filesystem::path dataDir_;
	std::filesystem::path oldDir_;
	std::filesystem::path importedFrom_;
	std::vector<entry> animeEntries_;
	std::vector<entry> mangaEntries_;
	provider provider_ = provider::kAniList;
	std::optional<isoDate> linkedOn_;

	// What the store holds, as far as this program knows: the lists and
	// settings as last read or written.
	std::vector<entry> savedAnime_;
	std::vector<entry> savedManga_;
	provider savedProvider_ = provider::kAniList;
	std::optional<isoDate> savedLinkedOn_;

	// The write is declared after the database: it must end first.
	store::database db_;
	std::optional<store::transaction> write_;
};

}  // namespace catalist

#endif  // CATALIST_CATALOGUE_HPP_
