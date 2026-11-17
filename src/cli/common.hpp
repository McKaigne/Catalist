#ifndef CATALIST_CLI_COMMON_HPP_
#define CATALIST_CLI_COMMON_HPP_

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "cli/cli.hpp"
#include "cli/output.hpp"
#include "core/entry.hpp"
#include "core/lists.hpp"
#include "core/result.hpp"
#include "storage/journal.hpp"

// What the commands of the command line share, and the commands
// themselves: each file named below holds a few commands, their handlers
// and their rows of the table. cli.cpp puts the table together and runs it.
namespace catalist::cli {

// ---- Messages ----

// "error: <message>" on standard error; returns 1, the exit code of a
// failure.
int Fail(CliContext& ctx, const std::string& message);
// The message, the command's usage line and where to read more; returns 2,
// the exit code of a wrong command line.
int UsageError(CliContext& ctx, const Command& cmd, const std::string& message);
// " (did you mean 'ls'?)", or "" when nothing known is close.
[[nodiscard]] std::string DidYouMean(const std::string& typed,
                                     const std::vector<std::string>& known);
int SaveFailed(CliContext& ctx);

// ---- A title as text ----

// The columns of a title, in the order every tab-separated line and JSON
// object has them. A new column goes at the end: scripts count on the order.
extern const std::vector<std::string> kColumns;
[[nodiscard]] std::vector<output::Cell> CellsOf(const Entry& e);
// Everything about a title: the columns of kColumns, then those of
// kFullColumns (total_volumes, released, anilist_id, mal_id, url on `site`,
// kitsu_id), as show and the exports print it.
extern const std::vector<std::string> kFullColumns;
[[nodiscard]] std::vector<std::string> AllColumns();
[[nodiscard]] std::vector<output::Cell> FullCellsOf(const Entry& e, Provider site);
// The terminal's colour for each status.
[[nodiscard]] const char* StatusColor(WatchStatus status);
// "episodes", "chapter".
[[nodiscard]] std::string UnitName(MediaType type, int count = 2);
// "1 title", "3 titles".
[[nodiscard]] std::string Titles(size_t count);
// "5/28", or "5/?" while the total is not known.
[[nodiscard]] std::string OutOf(int done, int total);
// Rows as JSON Lines, or as tab-separated lines (the column names first
// when `header`). A write that fails, because the reader has gone, ends it.
void WriteRows(CliContext& ctx, const std::vector<std::string>& names,
               const std::vector<std::vector<output::Cell>>& rows, bool json, bool header);

// The statuses in the order they are listed, and the words they are
// written with: watching, completed, on-hold, dropped, planned and
// rewatching.
extern const std::vector<WatchStatus> kStatuses;
[[nodiscard]] std::vector<std::string> StatusWords();
// A status as the command line takes it: one of StatusWords(), "reading"
// for watching or "rereading" for rewatching, in capitals or not. Nothing for any other word:
// the command line guesses nothing (import reads the sites' spellings).
[[nodiscard]] std::optional<WatchStatus> StatusNamed(const std::string& word);

// ---- Naming titles ----

// The operands of a command that takes one text (ls, search and add) as
// that text: the words with a space between them, so a title of several
// words needs no quotes. Empty without operands. Nothing, after the usage
// error (exit code 2), on a later word that looks like an option: options
// come before the text.
[[nodiscard]] std::optional<std::string> ReadText(const Command& cmd, const args::Parsed& line,
                                                  CliContext& ctx);

// -a and -m: the one list a command is kept to, or both.
[[nodiscard]] std::optional<MediaType> OnlyType(const args::Parsed& line);

// How the REFs of a command are matched: in which list, whether a REF may
// stand for several titles, and whether the person can be asked which.
struct Naming {
	std::optional<MediaType> only;
	bool all = false;
	bool ask = false;
};

// The options of every command that takes REFs: -a, -m and --all first.
[[nodiscard]] std::vector<args::Option> WithRefOptions(std::vector<args::Option> options);

// The REFs given, or the exit code when there is nothing to go on with.
struct RefRequest {
	std::vector<std::string> refs;
	Naming how;
	std::optional<int> stop;
};

// Reads the REF operands; a `-` among them stands for the lines of standard
// input, which then cannot answer a question as well.
[[nodiscard]] RefRequest ReadRefs(const Command& cmd, const args::Parsed& line, CliContext& ctx);

// Asks which of the numbered choices was meant; nothing when the answer is
// empty or not one of them.
[[nodiscard]] std::optional<size_t> AskWhich(CliContext& ctx, size_t count);

// Every title the REFs name, each once, in the order named. Nothing, with
// the reason on standard error, when any REF fails: a command works on all
// it was given, or on none.
[[nodiscard]] std::optional<std::vector<Entry>> Resolve(CliContext& ctx,
                                                        const std::vector<std::string>& given,
                                                        const Naming& how);

// Reads the lists; false, with the reason on standard error, when they
// cannot be read.
[[nodiscard]] bool Load(CliContext& ctx);
// Takes the catalogue for writing, then reads it: nothing another program
// does can come between what the command reads and what it saves.
[[nodiscard]] bool LoadForWrite(CliContext& ctx);

// ---- Changing titles ----

// A line for every field that changed. Into a pipe: the ref, the title, the
// field, its old value and its new one.
void PrintChanges(CliContext& ctx, const std::vector<journal::Change>& changes);

// Saves what a command changed as one undo step and prints it (not with
// -q), then tells the sites (unless `tell` is false: the command does). A
// command that changed nothing says so and saves nothing.
int SaveAndReport(CliContext& ctx, const args::Parsed& line,
                  const std::vector<journal::Change>& changes, bool tell = true);

using TitleChange = std::function<Entry(const Entry&)>;

// Makes one change to each title, then saves and reports it.
int ApplyTo(CliContext& ctx, const args::Parsed& line, const std::vector<Entry>& titles,
            const TitleChange& change);

// Makes one change to every title the REFs name; every REF is resolved
// before any title changes.
int ChangeTitles(const Command& self, const args::Parsed& line, CliContext& ctx,
                 const TitleChange& change);

// The N of -n, or `otherwise` when it is not given.
[[nodiscard]] Result<int> CountGiven(const args::Parsed& line, int otherwise);

// ---- The sites ----

// --from SITE: a site to ask other than the provider.
extern const args::Option kFrom;
// Checks the site --from names. False, after the usage error, when it names
// none.
[[nodiscard]] bool FromKnown(const Command& cmd, const args::Parsed& line, CliContext& ctx);
// Whether the command needs the catalogue read to know its site: no --from.
[[nodiscard]] bool AsksProvider(const args::Parsed& line);
// The site a command asks: --from's, else the provider (read the catalogue
// first).
[[nodiscard]] Provider SiteOf(const args::Parsed& line, const CliContext& ctx);
// The site's adapter, reached through the context's link.
[[nodiscard]] std::unique_ptr<web::Tracker> TrackerFor(const CliContext& ctx, Provider site);
// A search of the site, with Mihon's two forms besides the words: "id:N",
// the title with the site's id N; and "my:WORDS", the titles of your own
// list there (logged in) that hold the words.
[[nodiscard]] web::Answer<std::vector<web::Match>> SearchSite(CliContext& ctx, web::Tracker& site,
                                                              MediaType type,
                                                              const std::string& query);
// The match to take of a search on the site: the only one, the one `pick`
// numbers, or the one chosen at a terminal. Nothing, with the reason on
// standard error, when there is no such match.
[[nodiscard]] std::optional<web::Match> ChooseMatch(CliContext& ctx, web::Tracker& site,
                                                    MediaType type, const std::string& query,
                                                    std::optional<int> pick);

// What to say when a request to a site failed: when it could not be
// reached, to check the connection.
[[nodiscard]] std::string WebError(const web::Failure& why);

// AniList, MyAnimeList and Kitsu, in the order they are listed.
[[nodiscard]] const std::vector<Provider>& AllSites();
// Your entry on a site as a title of the lists (its score out of 10), for
// MergeLinked and the like.
[[nodiscard]] Entry AsTitle(const web::SiteEntry& theirs, Provider site);
// Takes the catalogue for writing and reads the title again, as it is now.
// Nothing, with the reason on standard error, when it is gone meanwhile.
[[nodiscard]] std::optional<Entry> HeldAgain(CliContext& ctx, const Entry& title);

// The logins, from the accounts file (none when there is no file).
[[nodiscard]] std::vector<Account> ReadAccounts(const CliContext& ctx);
// Writes them, readable by their owner only; false when that failed.
[[nodiscard]] bool SaveAccounts(const CliContext& ctx, const std::vector<Account>& logins);

// ---- Telling the sites (sync.cpp) ----

// What a send did: how many titles the sites were told of, and how many
// still wait.
struct SendReport {
	size_t told = 0;
	size_t left = 0;
	// Refused too often to try again (see SendTo).
	size_t gave_up = 0;
};
// Tells the logged-in sites what waits in the outbox, oldest first: all of
// it with `everything`, else what is due (not tried a short while ago) as
// long as it is a few. The lists are read.
SendReport SendOutbox(CliContext& ctx, bool everything);
// "note: 3 changes wait to be sent", when some do.
void NoteWaiting(CliContext& ctx, const SendReport& report);
// Sends what waits and is due, when anyone is logged in: the commands that
// go online anyway send it on the way, as Mihon retries once it is online.
void SendWaiting(CliContext& ctx);
// After a save: queues each changed title for every logged-in site where
// it is linked, and sends what waits.
void TellSites(CliContext& ctx, const std::vector<journal::Change>& changes);
// Queues what changed in each title for every logged-in site where it is
// linked but `skip`, sending nothing; false when it could not be kept.
[[nodiscard]] bool QueueChanges(CliContext& ctx, const std::vector<journal::Change>& changes,
                                std::optional<Provider> skip);
// After rm --remote: queues the titles to come off the sites' lists.
void TellSitesGone(CliContext& ctx, const std::vector<Entry>& gone);

// What the helps share.
extern const args::Option kQuiet;
extern const char* const kRefsHelp;
extern const char* const kChangeFormat;

// ---- The commands, by file ----

// listing.cpp: ls, show and stats, which only read.
[[nodiscard]] std::vector<Command> ListingCommands();
// What `ls` lists, and how.
struct ListQuery {
	// Words the titles hold.
	std::string text{};
	// Any of these; all of them when empty.
	std::vector<WatchStatus> statuses{};
	std::optional<MediaType> only{};
	// Only titles with an id on each of these sites, and none on any of
	// those.
	std::vector<Provider> linked{};
	std::vector<Provider> unlinked{};
	SortKey key = SortKey::kTitle;
	bool reverse = false;
	// The refs alone, one a line.
	bool refs_only = false;
	bool json = false;
	bool header = false;
};
// Lists the titles the query asks for: a table at a terminal, else lines.
// The exit code: 1 when the lists cannot be read.
int PrintList(CliContext& ctx, const ListQuery& query);
// tracking.cpp: +, -, set, done, hold, drop, plan, resume, rewatch and rm.
[[nodiscard]] std::vector<Command> TrackingCommands();
// The fields `set` takes, for the completion scripts.
[[nodiscard]] std::vector<std::string> SettableNames();
// online.cpp: add, search and refresh, which ask a site about titles.
[[nodiscard]] std::vector<Command> OnlineCommands();
// accounts.cpp: login, logout and accounts.
[[nodiscard]] std::vector<Command> AccountCommands();
// sync.cpp: sync, link and unlink.
[[nodiscard]] std::vector<Command> SyncCommands();
// track.cpp: track, each site's entry of a title.
[[nodiscard]] std::vector<Command> TrackCommands();
// transfer.cpp: import and export.
[[nodiscard]] std::vector<Command> TransferCommands();
// history.cpp: undo, redo and log.
[[nodiscard]] std::vector<Command> HistoryCommands();
// setup.cpp: provider and completion, and the hidden commands of the
// completion scripts and the build.
[[nodiscard]] std::vector<Command> SetupCommands();

}  // namespace catalist::cli

#endif  // CATALIST_CLI_COMMON_HPP_
