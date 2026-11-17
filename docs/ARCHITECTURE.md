# Catalist: Architecture and Decisions

Catalist is a local-first anime and manga tracker for the command line, in
C++23. This page says how the program is put together and why: the module
map, then the decisions that hold today (D1 to D14), then a short history
of the ones that were replaced. A decision gives its reason and, where it
has one, its cost: what it makes harder, and when to revisit it.

## Module map
Each module is a `.hpp`/`.cpp` pair in a folder of `src/` by layer. A
layer uses only the layers above it in this table, and `system`.

| Folder | Module | Holds |
| :--- | :--- | :--- |
| `core/` | `date`, `result` | `isoDate` and the stamp of a step; `result<T>` and `done`, `std::expected` with an error message |
| | `account.hpp` | a login on a site: user, token, renewal, expiry |
| | `entry` | one anime or manga title, its statuses and the rules that change it (progress, status, score, counts, dates, import merge) |
| | `lists` | what a list of titles works out to: its order (`sortEntries`) and its numbers (`countByStatus`, `averageScore`, ...) |
| | `text` | UTF-8 helpers: glyphs, terminal width, clipping, lower case |
| `storage/` | `store` | a small RAII wrapper over SQLite (`database`, `statement`, `transaction`), the only code that includes `sqlite3.h` |
| | `catalogue` | the two lists in memory, kept in `catalist.db` with the Kitsu ids beside them, and the outbox of what the sites are still to be told; `history.cpp` holds its undo and redo |
| | `accounts` | the logins file: its place, and its lines in and out |
| | `csvCodec` | the CSV form of a title: the rows of the undo history, and the files of 0.3.0 and earlier that `import` reads |
| | `journal` | pure functions over the undo history: what changed between two lists, taking a step back or forward |
| | `malXml` | the MyAnimeList XML format lists are imported and exported in |
| `web/` | `http`, `json` | one request type, form and URL encoding; the JSON the sites answer in |
| | `tracker` | `iTracker`, the one interface every site implements (D12); `answer<T>`, `failure` and `checked`, what a site answers; `siteEntry`, your entry there; `sending`, what a push sends |
| | `anilist`, `mal`, `kitsu`, `sites` | the adapters of AniList, MyAnimeList and Kitsu, and `makeTracker`, which picks one |
| `cli/` | `args`, `refs`, `output` | the POSIX option parser; refs (`a:12`) and title matching; tables, tab-separated lines and JSON Lines |
| | `cli` | `cliContext`, the `command` row, the table put together, dispatch and help |
| | `common` | what the commands share (naming titles, statuses, loading, saving and reporting a change), and one function per group of commands |
| | `listing`, `tracking`, `online`, `accounts`, `sync`, `track`, `transfer`, `history`, `setup` | the commands, by group: their handlers and their rows of the table |
| | `completion` | the zsh and fish completions and the manual page's COMMANDS, written from the command table |
| `src/` | `system` | everything that touches the outside world: files, `runProgram` and `httpFetch` (curl), the clock, the terminal, random text |

`main.cpp` reads the environment, builds the catalogue on the data
directory and the `cliContext`, and runs the command line. Tests mirror the
folders; `tests/smoke/` runs the built program from `sh`, and
`tests/support/` holds the fixtures.

## Review, November 2026
The decisions below were reviewed against what the program is for: a small
tracker for one person, built as a C++ course project, that a script can
drive. Most hold up. Three carry a real cost and say so:

- **D2 No settings** holds, with two exceptions it names: the provider and
  the sites' app ids.
- **D9 One file with Catalist-TUI** is what makes the storage look
  roundabout (a side table for Kitsu ids, undo rows as CSV text). That is
  the price of sharing the file, and it is paid on purpose.
- **D12 Sites as trackers** is a third of the code, now at parity with
  Mihon's tracking, and has not been run against the live sites. It stays,
  documented as untested; it is the first thing to cut if the program must
  shrink.

## Principles

### D1: Local First; the Lists Are the Truth (was ADR-013, ADR-017)
- **Decision:** the lists are `catalist.db` on the person's computer and
  nothing else. The network is used only when a command asks a site
  (`add`, `search`, `refresh`, `import --user`, `login`, `sync`) or, logged
  in, to tell a site of a change (D12). Nothing about the network is
  needed to use the lists: `add --no-lookup` adds a title offline.
- **Ids:** an entry's id only numbers it within its list (a new title gets
  the highest id + 1); the command line shows it as the title's ref
  (`a:12`), which can come back after the title with the highest id is
  removed. Two titles are the same only when they share an AniList,
  MyAnimeList or Kitsu id.
- **Lists in and out:** `import` takes MyAnimeList XML, Catalist's own JSON
  Lines, 0.3.0's CSV files or a user's public lists on a site; `export`
  writes XML (which all three sites import), CSV or JSON. Importing merges
  by id: a title there already keeps what the person gave it and gets only
  what it lacks (`mergeImported`).
- **Why:** a tracker is used daily, offline as often as not, and must not
  depend on a site staying up or keeping its API.

### D2: No Settings (was ADR-011)
- **Decision:** the defaults are fixed in code. There is no configuration
  file; an old `settings.json` or `catalist.kdl` is ignored.
- **Exceptions:** the **provider** (AniList, MyAnimeList or Kitsu), kept
  in the `setting` table, is the default site of `search`, `add`,
  `refresh`, `import --user` and of `show`'s page link; `--from SITE`
  overrides it for one command. The sites' **app ids** are built in
  (`-DCATALIST_*_CLIENT_ID`) and the same names in the environment
  override them; they belong to the build, not the person.
- **Why:** every setting is a branch to test and a question to answer.
  Hardly anyone changed the ones that existed.

## Code

### D3: Code Reads Plainly (was ADR-001, ADR-006)
- **Comments say why:** names and small functions say *what* code does.
  Comments say *why*, state invariants, and label the parts of longer
  functions so they read in sections.
- **Shallow control flow:** function bodies nest at most three levels.
  Preconditions and errors return early; loops skip with `continue`; each
  distinct subtask is a small, named helper (`readRefs`, `chooseMatch`,
  `heldAgain`).

### D4: Naming and Formatting (was ADR-008)
- **Names:** type names are `PascalCase`. Function names are `PascalCase`.
  Variable names and parameters are `snake_case`. Data members are
  `snake_case_` with a trailing underscore. Constants and enumerators are
  `kPascalCase`. Namespaces are lower case. File names are lower case with
  underscores. Macros are `SCREAMING_SNAKE_CASE`. Interfaces have no
  special prefix. Accessors and mutators match the variable name
  (`count()`, `set_count()`).
- **Formatting:** `.clang-format` (Google-based): tabs four wide, a
  100-column limit, braces on the line of their statement. Logical
  operators are written `and`, `or`, `not`.
- **Idioms:** C++23. Strings are built with `std::format`; dates with
  `std::chrono` where it has the calendar. Rows of plain structs (the
  command table) name their fields with designated initialisers.
  Resources are owned by objects that release them (RAII). Standard
  algorithms replace hand-written loops where they say it more plainly.
  Includes name the folder (`#include "storage/catalogue.hpp"`), and
  header guards follow the path. Nothing is written to disk but
  `catalist.db` and the logins file (D9).

### D5: Types over Sentinels (was ADR-002)
- **Decision:** `std::optional<T>` for what may be absent, and `result<T>`
  / `done` (`std::expected` with a message) for what may fail; no `-1` or
  `"0000-00-00"`. A site's failure says what kind it is (`web::failure`:
  unreachable, not linked, refused) rather than leaving callers to read
  its message.

### D6: Side Effects in One Place (was ADR-005)
- **Decision:** `system.cpp` is the only code that touches file streams,
  other programs, the terminal or the system clock, as plain functions
  (`readFile`, `runProgram`, `httpFetch`, `localTime`, `isInteractive`,
  `terminalColumns`). `store.cpp` is the only code that includes
  `sqlite3.h`. Where a test must swap a side effect, the caller takes a
  function: a site takes a `web::link` (the fetch, the pause between
  requests, the clock and the app ids), and `cliContext` carries the clock
  and the link.
- **Composition root:** `main.cpp` reads the environment, builds the
  catalogue and the `cliContext`. There are no singletons.
- **Processes:** the only program run is curl, with `execvp` and an
  argument list, so nothing passes through a shell. A request body goes in
  on standard input, never on the command line.
- **Why:** interfaces with a real class and a fake each wrapped one or two
  calls; a function does the same job with less code.

## Data

### D7: One SQLite File, with an Undo History (was ADR-016)
- **Decision:** the lists, the provider and the history are one file,
  `catalist.db`, in `$CATALIST_DATA_DIR`, `$XDG_DATA_HOME/catalist` or
  `~/.local/share/catalist`. Tables: `entry` (one row a title), `setting`,
  `tx` (one row a step: stamp, label, undone) and `change` (an entry
  before and after). The default rollback journal, a 5 s busy timeout.
- **Saving:** the catalogue keeps a snapshot of what it loaded and writes
  only the rows that differ, in one transaction. A save with an undo note
  records the step; a new step deletes the undone ones, and the last 100
  are kept.
- **Locking:** a command that changes the lists takes the write lock
  (`beginWrite`, `BEGIN IMMEDIATE`) before it loads, so nothing another
  program does comes between what it reads and what it saves. A site is
  asked *before* the lock is taken (D11), so no program waits on the
  network.
- **Undo and redo:** `journal` computes the changes (`diff`) and takes a
  step per entry (`transition`): only the fields the step changed are
  checked and set, so what a site filled in since is kept. A title changed
  since stops the step unless forced; a removed title comes back under its
  old id, never over another title. Ids and released counts are no step.
- **Why:** two CSV files and a lock file could not hold a history safely,
  and two programs saving at once could lose a change.

### D8: Files of Earlier Versions Are Read by Import (was ADR-016, ADR-020)
- **Decision:** the CSV files of 0.3.0 and earlier are read only by
  `catalist import`, like any other list, and never changed. Nothing is
  imported automatically on a first run.
- **Why:** a first-run import threaded through the catalogue, the command
  line and the tests for something done once.

### D9: One File with Catalist-TUI; New Data in Tables of Its Own (was ADR-017, ADR-019)
- **Context:** Catalist-TUI, the full-screen program, shares
  `catalist.db`. It opens only `user_version` 1 and writes `entry` with a
  positional 17-value INSERT, so a new `entry` column or a new version
  would break it.
- **Decision:** the schema of D7 stays as it is. What this program adds
  goes in tables the TUI does not know, made with `CREATE TABLE IF NOT
  EXISTS` on write:
  - `entry_id` holds the Kitsu ids, with the MyAnimeList and AniList ids
    the title had when written: the TUI gives a removed title's ref to the
    next one, and a row whose ids no longer match is not read as that
    other title's.
  - `outbox` holds the titles the sites are still to be told of, with the
    fields to send, the tries that failed and when to try again (columns
    added to an older outbox with `ALTER TABLE` on the first write).
  - `track_option` holds whether a title is hidden on a site, with the
    site's id, for the same reason as `entry_id`.
  The undo rows may carry an 18th CSV field, `kitsu_id`, which the TUI's
  reader ignores. This program writes only the `provider` setting and
  leaves other keys (the TUI's `linked`) alone. The `rewatching` status is
  read by the TUI as watching, and written back as watching when the TUI
  saves the title; nothing else is lost.
- **Logins** are kept outside the database, in
  `$XDG_STATE_HOME/catalist/accounts` (or `~/.local/state/catalist`; with
  `CATALIST_DATA_DIR`, in that directory), mode 600, written beside and
  renamed into place. The lists can be copied and shared; the logins not.
  Passwords (Kitsu's) are exchanged for a token and never kept.
- **Verified** against Catalist-TUI 2f589e3 on one file: each opens, edits
  and saves it, keeps the other's data, and undoes the other's steps.
  Changes made in the TUI are not queued for the sites; `sync --all` sends
  them.
- **Cost:** the Kitsu id lives in a side table with a staleness check, the
  undo history stores entries as CSV text inside SQLite, and the provider
  is written as `myanimelist`, the spelling 0.3 and the TUI read. A schema of its own
  (version 2, a `kitsu_id` column, real columns in `change`) would remove
  all three. **Revisit** if Catalist-TUI is retired or moves to this
  program's storage code.

## Command line

### D10: Command Line Only, in Folders by Layer (was ADR-017)
- **Decision:** this program is the command line only; the full-screen
  interface is Catalist-TUI. `src/` has a folder per layer (`core`,
  `storage`, `web`, `cli`) and `system` beside `main.cpp`. The commands
  are grouped in files (`listing`, `tracking`, `online`, `accounts`,
  `sync`, `track`, `transfer`, `history`, `setup`), each with its handlers and its
  rows of the table, sharing `cli/common`; `cli.cpp` collects the groups in
  help order. A new command goes in the file of its group.
- **Why:** two ways in made each one harder to change. Two programs, each
  simple, share the file instead (D9).

### D11: The Command Line Is for Daily Tracking, Scripts and Pipes (was ADR-004, ADR-015)
- **One table:** each command is a row (name, aliases, summary, details,
  options with their values and choices, operands and their kind, handler,
  hidden). The row drives `help`, `--help`, typo suggestions, the usage
  lines, the manual page (`__man`) and the zsh and fish completions. The
  build runs the program to write the page and the scripts, so they cannot
  fall behind it.
- **POSIX options:** grouped short options, `-n3`, `--name=value` and
  `--name value`, `--`, and a lone `-`; options come before operands.
- **Refs:** a title is named by `a:12` / `m:3` or by part of its title; an
  exact title wins, several matches fail unless `--all`, and nothing is
  guessed. Statuses are likewise exact words (`watching`, `completed`,
  `on-hold`, `dropped`, `planned`, and `reading`); only `import` reads the
  sites' own spellings. A `-` operand reads refs from standard input.
- **Streams:** a command writes only its data to stdout; messages,
  questions and errors go to stderr. Exit codes: 0 done, 1 failed, 2 wrong
  command line. A failed write to stdout, as when `head` has gone, ends the
  command quietly with 0.
- **Output:** a table at a terminal; into a pipe, one tab-separated line a
  title, `--json` for JSON Lines, `--header` for the column names, `-q`
  for only the refs or nothing. A new column goes at the end.
- **Changing titles:** take the write lock, load, resolve every operand
  before changing anything, apply the entry rules, save with an undo note,
  print one line per changed field. A site is asked before the lock is
  taken (`add`, `refresh`, `link`).
- **No command:** `catalist` alone at a terminal lists what is being
  watched, new episodes first; into a pipe it is a usage error.
- **Completion:** the scripts ask the hidden `catalist __complete` for
  titles, statuses and fields. It opens the database read-only, waits at
  most 100 ms, creates nothing, and prints nothing on any failure.
- **Testing:** `cliContext` carries the streams, the clock, the
  `web::link` and whether a terminal is attached, so tests drive every
  command with string streams and a fake web.

## Sites

### D12: Sites as Equal Trackers, Told One Way, as Mihon Does (was ADR-018)
- **Context:** the owner wanted AniList, MyAnimeList and Kitsu to work the
  same, and tracking at parity with Mihon's (reading aside). Mihon tells
  each tracker what changes, one way; each series is linked to each
  tracker by its id there; every tracker does the same few jobs behind one
  interface. Its source (`AddTracks`, `TrackChapter`, `RefreshTracks`,
  `DelayedTrackingUpdateWorker`, the trackers) is what these rules follow.
- **Decision:** `web::iTracker` is that interface, and `anilist`, `mal`
  and `kitsu` each implement all of it: `search`, `lookUp`/`lookUpMany`,
  `startLogin`/`finishLogin`/`current`, `pull` (a user's public list),
  `entryOf` (your own entry of a title) and `push`/`remove`. `makeTracker`
  picks one; no command names a site in its code.
- **One entry, sent one way:** the lists are the truth, and each title is
  one entry, sent to every logged-in site where it is linked; a site's
  entry is never a second, editable copy. `track REF` shows each site's
  entry read live, and `track --pull SITE` takes one as the title's.
- **Binding:** `link` reads your entry on the site first and folds it in
  (`mergeLinked`: what you set stays, the higher progress wins, the site
  fills in what you have not set, its status replaces planned), one step
  for undo; the site then gets the title whole, as Mihon's `bind` and
  `AddTracks`.
- **Sending:** only the fields a change changed are sent (the outbox
  keeps them as `siteField` bits, merged when the title changes again), so
  what you changed on the site itself stays. A partial send reads the
  site's entry first, as `TrackChapter` refreshes before it updates: a site
  without the title gets all of it, and progress that only went up (`+`)
  does not lower a site further along. `-`, `set` and `undo` send what you
  set.
- **Retries:** the HTTP status decides (`web::checked`): no reply, 429 and
  5xx wait; 401 and 403 wait for a new login; other refusals wait too, and
  after three tries are given up (Mihon's worker stops after three).
  Waiting is five minutes, then twice as long each time, up to six hours;
  the next command that goes online sends what is due (`sendWaiting`), and
  `sync` sends everything at once. There is no background job.
- **Linking:** a title is linked to a site when it has that site's id.
  `add`, `refresh` and `import` keep every id a site gives (AniList gives
  MyAnimeList's; Kitsu's mappings give both), so linking is mostly
  automatic. `unlink --remote` and `rm --remote` take a title off the
  site's list too.
- **Known differences** stay behind the interface: only AniList says how
  much of a running show has aired; MyAnimeList answers nothing without the
  program's client id, and has no hidden entries; Kitsu has no volumes.
  Scores are kept and sent in each site's scale (AniList out of 100,
  Kitsu's ratingTwenty, MyAnimeList out of 10); `web::outOfTen` converts
  one only to draw it, as five stars like every score at a terminal.
- **Not taken from Mihon:** its settings (update after reading, ask first),
  its background job, half points, a separate entry per site, and the
  trackers other than these three.
- **Cost and status:** this is about a third of the code (`web/`,
  `cli/sync`, `cli/track`, `cli/accounts`, the outbox). It is tested
  against recorded answers shaped like Mihon's requests only: **no request
  has been made to the live sites**, the app ids are not registered yet,
  and Kitsu's endpoints, MyAnimeList's date clearing and Kitsu's
  `reconsuming` are unchecked. **Revisit:** if the program must shrink,
  tracking goes first, keeping `search`, `add`, `refresh` and
  `import --user`.

## Build, tests and releases

### D13: Few Dependencies, Little Machinery (was ADR-020)
- **Decision:** the build needs CMake, a C++23 compiler and SQLite, and
  doctest for the tests; nothing is downloaded, and curl is needed only at
  run time. `std::format` and `std::expected` replaced fmt and a hand-made
  result class. zsh and fish have completions; bash has none, and a
  shorter name is a shell alias away. The Nix flake builds the package and
  gives the `catalist-dev` shell, which keeps by-hand runs' lists in
  `build/dev-data`.
- **Rule:** a dependency, or a feature that threads through several
  layers, has to earn it; when the standard library or a shell does the
  job, it does.

### D14: Hermetic Tests That Check Behaviour (was ADR-010)
- **Decision:** doctest, registered with ctest one case at a time. The
  test `main` points `CATALIST_DATA_DIR`, `HOME`, `XDG_DATA_HOME` and
  `XDG_CONFIG_HOME` at a throwaway sandbox and pins `TZ=UTC`; tests that
  touch files get their own temporary directory, and a failed write is
  simulated by a directory where the file should go. Everything runs
  offline, with a fixed date and a fake web.
- **What a test checks:** what the person would see: the lists as another
  catalogue reads them back, the exit code and the output. No test is
  skipped or switched off.
- **Where:** unit tests mirror `src/`; command tests share `cliFixture`;
  `tests/smoke/smoke.sh` drives the built program from `sh` with a stub
  `curl`; ctest checks that the completions parse with `zsh -n` and
  `fish -n`. CI also runs the suite under AddressSanitizer and UBSan.
- **Versions** (was ADR-009): `VERSION` is the only place the number is
  written; CMake, the manual page, the flake and CI read it. PATCH for
  fixes, MINOR for new commands or options, MAJOR for big releases; 0.x
  until the first full release. Every bump adds a Keep a Changelog entry,
  is its own `release: vX.Y.Z` commit and gets an annotated tag. Internal
  work, such as the `simplify/cleanup` branch, does not bump.

## History
Decisions that were replaced, for reading old commits and the changelog:

- **ADR-003 Decoupled multi-provider subsystem:** web sources behind an
  `IProvider` interface and a `ProviderRegistry`. Replaced by AniList alone
  (ADR-014), then by D12.
- **ADR-007 Tabs behind `iTab`:** the full-screen interface's tabs. Moved
  with that interface to Catalist-TUI (D10).
- **ADR-012 Two-way list sync against a base:** accounts, tokens, a base
  per title and site, request pacing, backups and a job thread. Dropped as
  most of the code for a feature a local tracker did not need; one-way
  tracking (D12) later took its place.
- **ADR-013 Local first, one-way import:** its network rule ("only `add`
  and `search`") was widened by D12; the rest lives on in D1.
- **ADR-014 AniList only, a provider ready for sync:** `iListSync` was
  declared but never built. Replaced by D12; the provider lives on in D2.
- **ADR-001 to ADR-020** otherwise map to D1 to D14 as each heading notes.
