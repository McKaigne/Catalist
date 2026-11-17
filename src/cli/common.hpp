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
int fail(cliContext& ctx, const std::string& message);
// The message, the command's usage line and where to read more; returns 2,
// the exit code of a wrong command line.
int usageError(cliContext& ctx, const command& cmd, const std::string& message);
// " (did you mean 'ls'?)", or "" when nothing known is close.
[[nodiscard]] std::string didYouMean(const std::string& typed,
                                     const std::vector<std::string>& known);
int saveFailed(cliContext& ctx);

// ---- A title as text ----

// The columns of a title, in the order every tab-separated line and JSON
// object has them. A new column goes at the end: scripts count on the order.
extern const std::vector<std::string> kColumns;
[[nodiscard]] std::vector<output::cell> cellsOf(const entry& e);
// Everything about a title: the columns of kColumns, then those of
// kFullColumns (total_volumes, released, anilist_id, mal_id, url on `site`,
// kitsu_id), as show and the exports print it.
extern const std::vector<std::string> kFullColumns;
[[nodiscard]] std::vector<std::string> allColumns();
[[nodiscard]] std::vector<output::cell> fullCellsOf(const entry& e, provider site);
// The terminal's colour for each status.
[[nodiscard]] const char* statusColor(watchStatus status);
// "episodes", "chapter".
[[nodiscard]] std::string unitName(mediaType type, int count = 2);
// "1 title", "3 titles".
[[nodiscard]] std::string titles(size_t count);
// "5/28", or "5/?" while the total is not known.
[[nodiscard]] std::string outOf(int done, int total);
// Rows as JSON Lines, or as tab-separated lines (the column names first
// when `header`). A write that fails, because the reader has gone, ends it.
void writeRows(cliContext& ctx, const std::vector<std::string>& names,
               const std::vector<std::vector<output::cell>>& rows, bool json, bool header);

// The statuses in the order they are listed, and the words they are
// written with: watching, completed, on-hold, dropped, planned and
// rewatching.
extern const std::vector<watchStatus> kStatuses;
[[nodiscard]] std::vector<std::string> statusWords();
// A status as the command line takes it: one of statusWords(), "reading"
// for watching or "rereading" for rewatching, in capitals or not. Nothing for any other word:
// the command line guesses nothing (import reads the sites' spellings).
[[nodiscard]] std::optional<watchStatus> statusNamed(const std::string& word);

// ---- Naming titles ----

// The operands of a command that takes one text (ls, search and add) as
// that text: the words with a space between them, so a title of several
// words needs no quotes. Empty without operands. Nothing, after the usage
// error (exit code 2), on a later word that looks like an option: options
// come before the text.
[[nodiscard]] std::optional<std::string> readText(const command& cmd, const args::parsed& line,
                                                  cliContext& ctx);

// -a and -m: the one list a command is kept to, or both.
[[nodiscard]] std::optional<mediaType> onlyType(const args::parsed& line);

// How the REFs of a command are matched: in which list, whether a REF may
// stand for several titles, and whether the person can be asked which.
struct naming {
	std::optional<mediaType> only;
	bool all = false;
	bool ask = false;
};

// The options of every command that takes REFs: -a, -m and --all first.
[[nodiscard]] std::vector<args::option> withRefOptions(std::vector<args::option> options);

// The REFs given, or the exit code when there is nothing to go on with.
struct refRequest {
	std::vector<std::string> refs;
	naming how;
	std::optional<int> stop;
};

// Reads the REF operands; a `-` among them stands for the lines of standard
// input, which then cannot answer a question as well.
[[nodiscard]] refRequest readRefs(const command& cmd, const args::parsed& line, cliContext& ctx);

// Asks which of the numbered choices was meant; nothing when the answer is
// empty or not one of them.
[[nodiscard]] std::optional<size_t> askWhich(cliContext& ctx, size_t count);

// Every title the REFs name, each once, in the order named. Nothing, with
// the reason on standard error, when any REF fails: a command works on all
// it was given, or on none.
[[nodiscard]] std::optional<std::vector<entry>> resolve(cliContext& ctx,
                                                        const std::vector<std::string>& given,
                                                        const naming& how);

// Reads the lists; false, with the reason on standard error, when they
// cannot be read.
[[nodiscard]] bool load(cliContext& ctx);
// Takes the catalogue for writing, then reads it: nothing another program
// does can come between what the command reads and what it saves.
[[nodiscard]] bool loadForWrite(cliContext& ctx);

// ---- Changing titles ----

// A line for every field that changed. Into a pipe: the ref, the title, the
// field, its old value and its new one.
void printChanges(cliContext& ctx, const std::vector<journal::change>& changes);

// Saves what a command changed as one undo step and prints it (not with
// -q). A command that changed nothing says so and saves nothing.
int saveAndReport(cliContext& ctx, const args::parsed& line,
                  const std::vector<journal::change>& changes);

using titleChange = std::function<entry(const entry&)>;

// Makes one change to each title, then saves and reports it.
int applyTo(cliContext& ctx, const args::parsed& line, const std::vector<entry>& titles,
            const titleChange& change);

// Makes one change to every title the REFs name; every REF is resolved
// before any title changes.
int changeTitles(const command& self, const args::parsed& line, cliContext& ctx,
                 const titleChange& change);

// The N of -n, or `otherwise` when it is not given.
[[nodiscard]] result<int> countGiven(const args::parsed& line, int otherwise);

// ---- The sites ----

// --from SITE: a site to ask other than the provider.
extern const args::option kFrom;
// Checks the site --from names. False, after the usage error, when it names
// none.
[[nodiscard]] bool fromKnown(const command& cmd, const args::parsed& line, cliContext& ctx);
// Whether the command needs the catalogue read to know its site: no --from.
[[nodiscard]] bool asksProvider(const args::parsed& line);
// The site a command asks: --from's, else the provider (read the catalogue
// first).
[[nodiscard]] provider siteOf(const args::parsed& line, const cliContext& ctx);
// The site's adapter, reached through the context's link.
[[nodiscard]] std::unique_ptr<web::iTracker> trackerFor(const cliContext& ctx, provider site);
// The match to take of a search on the site: the only one, the one `pick`
// numbers, or the one chosen at a terminal. Nothing, with the reason on
// standard error, when there is no such match.
[[nodiscard]] std::optional<web::match> chooseMatch(cliContext& ctx, web::iTracker& site,
                                                    mediaType type, const std::string& query,
                                                    std::optional<int> pick);

// What to say when a request to a site failed: when it could not be
// reached, to check the connection.
[[nodiscard]] std::string webError(const web::failure& why);

// The logins, from the accounts file (none when there is no file).
[[nodiscard]] std::vector<account> readAccounts(const cliContext& ctx);
// Writes them, readable by their owner only; false when that failed.
[[nodiscard]] bool saveAccounts(const cliContext& ctx, const std::vector<account>& logins);

// ---- Telling the sites (sync.cpp) ----

// What a send did: how many titles the sites were told of, and how many
// still wait.
struct sendReport {
	size_t told = 0;
	size_t left = 0;
};
// Tells the logged-in sites what waits in the outbox, oldest first: all of
// it with `everything`, else as long as it is a few. The lists are read.
sendReport sendOutbox(cliContext& ctx, bool everything);
// "note: 3 changes wait to be sent", when some do.
void noteWaiting(cliContext& ctx, const sendReport& report);
// After a save: queues each changed title for every logged-in site where
// it is linked, and sends what waits.
void tellSites(cliContext& ctx, const std::vector<journal::change>& changes);
// After rm --remote: queues the titles to come off the sites' lists.
void tellSitesGone(cliContext& ctx, const std::vector<entry>& gone);

// What the helps share.
extern const args::option kQuiet;
extern const char* const kRefsHelp;
extern const char* const kChangeFormat;

// ---- The commands, by file ----

// listing.cpp: ls, show and stats, which only read.
[[nodiscard]] std::vector<command> listingCommands();
// What `ls` lists, and how.
struct listQuery {
	// Words the titles hold.
	std::string text{};
	// Any of these; all of them when empty.
	std::vector<watchStatus> statuses{};
	std::optional<mediaType> only{};
	// Only titles with no id on this site.
	std::optional<provider> unlinked{};
	sortKey key = sortKey::kTitle;
	bool reverse = false;
	// The refs alone, one a line.
	bool refsOnly = false;
	bool json = false;
	bool header = false;
};
// Lists the titles the query asks for: a table at a terminal, else lines.
// The exit code: 1 when the lists cannot be read.
int printList(cliContext& ctx, const listQuery& query);
// tracking.cpp: +, -, set, done, hold, drop, plan, resume, rewatch and rm.
[[nodiscard]] std::vector<command> trackingCommands();
// The fields `set` takes, for the completion scripts.
[[nodiscard]] std::vector<std::string> settableNames();
// online.cpp: add, search and refresh, which ask a site about titles.
[[nodiscard]] std::vector<command> onlineCommands();
// accounts.cpp: login, logout and accounts.
[[nodiscard]] std::vector<command> accountCommands();
// sync.cpp: sync, link and unlink.
[[nodiscard]] std::vector<command> syncCommands();
// transfer.cpp: import and export.
[[nodiscard]] std::vector<command> transferCommands();
// history.cpp: undo, redo and log.
[[nodiscard]] std::vector<command> historyCommands();
// setup.cpp: provider and completion, and the hidden commands of the
// completion scripts and the build.
[[nodiscard]] std::vector<command> setupCommands();

}  // namespace catalist::cli

#endif  // CATALIST_CLI_COMMON_HPP_
