# Catalist Architectural Decision Records (ADRs)

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
| | `tracker` | `iTracker`, the one interface every site implements (ADR-018): search, look titles up, log in, read a user's list, push a title, remove one; `answer<T>` and `failure`, what a site answers |
| | `anilist`, `mal`, `kitsu`, `sites` | the adapters of AniList, MyAnimeList and Kitsu, and `makeTracker`, which picks one |
| `cli/` | `args`, `refs`, `output` | the POSIX option parser; refs (`a:12`) and title matching; tables, tab-separated lines and JSON Lines |
| | `cli` | `cliContext`, the `command` row, the table put together, dispatch and help |
| | `common` | what the commands share (naming titles, loading, saving and reporting a change), and one function per group of commands |
| | `listing`, `tracking`, `online`, `accounts`, `sync`, `transfer`, `history`, `setup` | the commands, by group: their handlers and their rows of the table (ADR-017) |
| | `completion` | the zsh and fish completions and the manual page's COMMANDS, written from the command table |
| `src/` | `system` | everything that touches the outside world: reading and writing files, `runProgram` and `httpFetch` (curl), the clock (`localTime`), the terminal, random text |

`main.cpp` reads the environment, builds the catalogue on the data
directory and the `cliContext`, and runs the command line. Tests mirror the
folders (`tests/core/`, `tests/storage/`, ...); `tests/smoke/` runs the
built program from `sh`, and `tests/support/` holds the fixtures.

## ADR-001: Comments Say Why, and Label Long Sections
- **Context:** Inline comments explaining step-by-step logic drifted over time.
- **Decision:** Names and small functions say *what* code does. Comments say
  *why*, state invariants and thread-safety contracts, and label the parts
  of longer functions ("The details card: ...") so they read in sections.

## ADR-002: Strong Types and Optionality over Magic Sentinels
- **Context:** Magic values (`-1`, `"0000-00-00"`) caused error-prone checks.
- **Decision:** Use `std::optional<T>` and `result<T>` (`std::expected`)
  instead of magic sentinels. A site's failure says what kind it is
  (`web::failure`: unreachable, not linked, refused) rather than leaving
  callers to read its message.

## ADR-003: Decoupled Multi-Provider Subsystem (superseded)
- **Superseded** by ADR-013 and ADR-014: the only web source is AniList,
  in `web/anilist`.

## ADR-004: Explicit Unix Filter and Standard Stream Discipline
- **Context:** Terminal tools must integrate smoothly into shell pipelines.
- **Decision:** a command writes only its data to `stdout`; messages,
  warnings, questions and errors go to `stderr`, and the exit code says
  what happened (0 done, 1 failed, 2 wrong command line). ADR-015 has the
  rest of the command line's contract.

## ADR-005: Side Effects in One Place, Passed as Functions Where Tests Swap Them
- **Context:** Code that reads files, the clock or the network cannot be
  tested without a home directory, a fixed date or a connection. Interfaces
  with a real class and a fake for each (`iFileSystem`, `iHttpClient`,
  `iClock`) did the job, but each wrapped only one or two calls.
- **Decision:** `system.cpp` is the only code that touches file streams,
  other programs, the terminal or the system clock, as plain functions:
  `readFile`, `runProgram`, `httpFetch`, `localTime`, `isInteractive`,
  `terminalColumns`. The database file is the other side effect, and
  `store.cpp` is the only code that includes `sqlite3.h`. Where a test must
  swap one, the caller takes a function instead: a site takes a
  `web::link` (the `fetch` function and the pause between requests), and
  `cliContext` carries one `clock` (its `today()` and `stamp()` come from
  it) and the link. The catalogue opens its directory directly; tests give
  it a temporary one.
- **Composition root:** `main.cpp` reads the environment into a
  `dataDirEnv` (`dataDirFor` is pure), builds the catalogue on the data
  directory, and fills the `cliContext`. There are no singletons.
- **Processes:** the only program run is curl, with `execvp` and an
  argument list, so nothing passes through a shell. `runProgram` writes
  the input and reads the output in one `poll` loop, without a thread.

## ADR-006: Shallow Control Flow
- **Decision:** Function bodies nest at most three levels. Preconditions and
  errors return early; loops skip with `continue`; each distinct subtask is a
  small, named helper (`readRefs`, `chooseMatch`, `removalConfirmed`).

## ADR-007: Tabs Behind `iTab`, Drawn and Routed by Hand (superseded)
- **Superseded** by ADR-017: the terminal interface, and with it `iTab`,
  moved to Catalist-TUI.

## ADR-008: Naming and Formatting
- **Names:** types, functions, variables and files are camelCase;
  interfaces are `i` + PascalCase (`iTracker`); private members end in
  `_`; constants and enumerators are `kPascalCase` (`mediaType::kAnime`,
  `output::kBold`). Namespaces stay lower case and macros
  SCREAMING_SNAKE_CASE. Names are descriptive (`widthPx`, `changedTitles`).
- **Formatting:** `.clang-format` (Google-based): tabs for indentation,
  four wide, a 100-column limit, opening braces on the line of their
  statement. Logical operators are written `and`, `or`, `not`.
- **Idioms:** C++23. Strings are built with `std::format`, not by
  concatenation; a value or an error is a `std::expected`
  (`result<T>`, `done`); rows of plain structs, such as the command table,
  name their fields with designated initialisers;
  resources are owned by objects that release them (RAII, e.g. the pipe
  ends in `runProgram`); the standard algorithms replace hand-written loops
  where they say it more plainly. A header is only added when nothing
  already included does the job. Includes name the folder
  (`#include "storage/catalogue.hpp"`), and header guards follow the path
  (`CATALIST_STORAGE_CATALOGUE_HPP_`). Nothing is written to disk but
  `catalist.db` (and SQLite's own journal beside it while it writes).

## ADR-009: One Version, Bumped Only for User-Visible Change
- **Decision:** `VERSION` holds `MAJOR.MINOR.PATCH` and is the only place
  the number is written. CMake reads it into `project(VERSION)`, generates
  `core/version.hpp` (`kVersionString`), and the program puts it in the
  man page; `flake.nix` reads it too, and CI checks `catalist --version` against it.
  PATCH is for fixes and small tweaks, MINOR for new commands or options (PATCH resets), MAJOR for big releases; the project stays on
  0.x until the first full-feature release. Internal refactors do not bump.
  Every bump adds a Keep a Changelog entry to `CHANGELOG.md`, is its own
  `release: vX.Y.Z` commit and gets an annotated tag.

## ADR-010: Hermetic Tests That Check Behaviour
- **Decision:** Tests use doctest and are registered with ctest one case at a
  time. The test `main` points `CATALIST_DATA_DIR`, `HOME`,
  `XDG_DATA_HOME` and `XDG_CONFIG_HOME` at a throwaway sandbox and pins
  `TZ=UTC` before any test code runs; each test that touches files gets its own temp directory, and
  a failed write is simulated by a directory where the file should go.
  Everything runs offline: tests pass a fixed date and a `fakeWeb`
  (ADR-005).
- **What a test checks:** the result the user would see: the lists as
  another catalogue reads them back, the exit code and the output of a
  command. No test is skipped, expected to fail or switched off by an
  environment variable.
- **Where:** the unit tests mirror `src/` (`tests/cli/listingTest.cpp`
  tests `src/cli/listing.cpp`); the command tests share `cliFixture`
  (`tests/support/cliFixture.hpp`), which runs `catalist ...` against a
  temporary directory.
- **From a shell:** `tests/smoke/smoke.sh` drives the built program as a
  POSIX script does, run by `sh`, with a stub `curl` first in `PATH` so
  nothing goes online. ctest also checks that the generated completions
  parse, with `zsh -n` and `fish -n` when those shells are installed.
- **Targets:** one library (`catalistLib`, everything but `main.cpp`), the
  program and one test binary (`catalistTests`); `-DBUILD_TESTING=OFF`
  builds without doctest. CI also runs the suite under AddressSanitizer and
  UBSan.

## ADR-011: No Settings
- **Context:** A settings tab, `settings.json`, themes and `catalist config`
  existed so users could change defaults that hardly anyone changed.
- **Decision:** The defaults are fixed in code (Helix-style). There is no
  configuration file; an old `settings.json` or `catalist.kdl`
  is ignored. The provider (ADR-014) is the one choice kept, with the
  lists.

## ADR-012: Two-Way List Sync Against a Base (superseded)
- **Superseded** by ADR-013. Sync needed accounts, tokens, a base per title
  and service, request pacing, backups and a job thread: most of the code,
  for a feature a local tracker does not need.

## ADR-013: Local First, One-Way Import (partly superseded)
- **Superseded** where it says the network is only for `add` and `search`,
  and that lists only come in by file: see ADR-018.
- **Decision:** The catalogue is `catalist.db` and nothing else (ADR-016).
  Lists
  come in with `catalist import` (MyAnimeList XML; AniList lists after
  converting them to that format) and go out with `catalist export`; neither
  touches an account. Re-importing merges by AniList or MyAnimeList id and never
  overwrites the user's score, progress or dates.
- **Network:** only `add` and `search` ask AniList, one request at a time,
  while the command runs (ADR-015). Nothing about the network is needed to
  use the lists: `add --no-lookup` adds a title offline.
- **Ids:** an entry's id only numbers it within its list (a new title gets
  the highest id + 1); the command line shows it as the title's ref
  (`a:12`), which can be given again after the title with the highest id
  is removed. Two titles are the same only when they share an AniList or a
  MyAnimeList id.

## ADR-014: AniList Only; a Provider, Ready for Sync (superseded)
- **Superseded** by ADR-018: AniList, MyAnimeList and Kitsu are equal
  sites behind `iTracker`; `iListSync` and `sync.hpp` are gone.
- **Context:** Jikan (MyAnimeList's unofficial API) could not be reached
  from some networks, and many users have moved to AniList. AniList's
  public API needs no account and also returns every title's MyAnimeList
  id.
- **Decision:** AniList is the only web source. Entries store both ids. The
  user picks one provider (AniList by default, or MyAnimeList); its ids are
  the ones a title's page link (`show`) uses. The provider lives in the catalogue (`getProvider`, `setProvider`,
  `saveSettings`, the `setting` table of `catalist.db`).
- **Room for sync:** `sync.hpp` declares `iListSync` (`pull`, `push`,
  `remove`) for one site. A sync would be one class per site, made for the
  chosen provider only, matching titles with `idOn(entry, site)`, its token
  kept outside the catalogue. Nothing implements it yet.

## ADR-015: The Command Line Is for Daily Tracking, Scripts and Pipes
- **Context:** the first list-manager CLI named titles by the provider's
  id, took options anywhere, looked titles up before every change, and
  could not be undone, so it was neither quick to type nor safe to script.
- **One table:** each command is a row of the table (name, aliases,
  summary, details, options with their values and choices, operands and
  their kind, handler, hidden). The row drives `help`, each `--help`, typo
  suggestions, the usage lines, the manual page (`__man` prints its
  COMMANDS, which the build puts into `docs/catalist.1.in`), and the zsh
  and fish completions (`completion`). The build runs the program to write
  the page and the scripts, so they cannot fall behind it.
- **POSIX options:** `args` parses grouped short options, `-n3`,
  `--name=value` (and `--name value`), `--`, and a lone `-`; options come
  before operands. A wrong command line exits with 2 and the usage line.
- **Refs:** a title is named by `a:12` / `m:3` (its local id) or by part of
  its title; an exact title wins, several matches fail unless `--all`, and
  nothing is guessed. A `-` operand reads refs from standard input.
- **Output:** a table at a terminal; into a pipe, one tab-separated line a
  title (a tab or newline inside a field becomes a space), `--json` for
  JSON Lines, `--header` for the column names, `-q` for only the refs or
  nothing. A failed write to `stdout`, as when `head` has gone, ends the
  command quietly with 0.
- **Network:** synchronous, through the site's `iTracker` (ADR-018): `add`,
  `search`, `refresh`, `import --user`, `login`, `sync`, and, logged in,
  the push after a saved change. A site is asked before the write lock is
  taken. `add` fails offline unless `--no-lookup`; a push that fails waits
  in the outbox, never failing the command that saved the change.
- **Mutating commands:** take the write lock (`beginWrite`), load, resolve
  every operand before changing anything, apply the entry rules
  (`setStatus`, `setScore`, `stepProgress`, `setCount`),
  save with an undo note, then print one line per changed field.
- **No command:** `catalist` alone at a terminal lists what is being
  watched (`printList`, as `ls -s watching --sort=new` would); into a pipe
  it prints its usage on stderr and exits with 2.
- **Completion:** the scripts know the commands and options by heart and
  ask the hidden `catalist __complete refs|titles|statuses|fields` for the
  rest. It opens the database read-only (`catalogue::peek`): no lock is
  waited for beyond 100 ms, nothing is created, and any failure prints
  nothing and exits with 0.
- **Testing:** `cliContext` carries the streams, the clock, the
  `web::link` and whether a terminal is attached, so tests drive every
  command with string streams and a fake web (ADR-010).

## ADR-016: One SQLite File, with an Undo History
- **Context:** two CSV files and a `settings` file could not hold an undo
  history safely: undo and redo by rewriting files, with a separate lock
  file, would be fragile, and two programs saving at once could lose a
  change.
- **Decision:** the lists, the settings and the history are one SQLite
  file, `catalist.db`, in `$CATALIST_DATA_DIR`, `$XDG_DATA_HOME/catalist`
  or `~/.local/share/catalist`. Tables: `entry` (one row a title, keyed by
  type and id), `setting`, `tx` (one row a step: stamp, label, undone) and
  `change` (an entry before and after, in the 17-column CSV encoding). The
  default rollback journal is used, with a 5 s busy timeout.
- **Saving:** the catalogue keeps a snapshot of what it loaded and writes
  only the rows that differ, in one transaction. A save with an undo note
  also records the step; a new step deletes the undone ones (no redo after
  new work), and only the last 100 are kept. `beginWrite` starts the
  transaction before the load (`BEGIN IMMEDIATE`), so nothing another
  program does comes between what a command reads and what it saves.
- **Undo and redo:** `journal` computes the changes (`diff`) and takes a
  step back or forward per entry (`transition`): only the fields the step
  changed are checked and set, so web data found since is kept. A title
  changed since stops the step unless forced; a removed title comes back
  under its old id, never over another title.
- **Old files:** the CSV files of 0.3.0 and earlier are read by
  `catalist import`, as any other list is (ADR-020); they are never
  changed.

## ADR-017: Command Line Only, in Folders by Layer
- **Context:** the program had two ways in, a full-screen interface (FTXUI,
  covers through img2sixel, a worker thread keeping totals current) and the
  command line. The owner wants two programs, each simple: this one for the
  command line, Catalist-TUI for the interface.
- **Decision:** this program is the command line only. FTXUI, libsixel, the
  updater thread, covers, the daily id look-ups and the interface's entry
  rules (status cycle, one star more) are gone; `text` measures glyph
  widths with its own table instead of FTXUI's. `catalist` alone at a
  terminal lists what is being watched; into a pipe it is a usage error.
- **One database, two programs:** `catalist.db` keeps the schema of
  ADR-016, so Catalist-TUI can work on the same file. This program writes
  only the `provider` setting and leaves other keys (Catalist-TUI's
  `linked`) as they are. Totals and released counts change when a title
  is added, imported, refreshed (`refresh`) or set by hand here, or when
  Catalist-TUI checks it. What this program adds to the file goes in
  tables of its own (ADR-019).
- **Layout:** `src/` has a folder per layer (`core`, `storage`, `web`,
  `cli`) and `system` beside `main.cpp`; see the module map. The commands
  are grouped in files (`listing`, `tracking`, `online`, `transfer`,
  `history`, `setup`), each with its handlers and its rows of the table,
  sharing `cli/common`; `cli.cpp` collects the groups into the table in
  help order and runs it. A new command goes in the file of its group, or
  a new group file listed in `common.hpp` and `commands()`.

## ADR-018: Sites as Equal Trackers, Told One Way
- **Context:** AniList, MyAnimeList and Kitsu were each reached a different
  way: AniList for search and look-ups, MyAnimeList by file only, Kitsu
  not at all. The owner wanted all three to work the same, and pointed at
  Mihon, which supports eight trackers with little code.
- **Why Mihon's way is small:** tracking is one way (the app tells the
  tracker, never merges back), each title is linked to each tracker by
  its id there, every tracker does the same few jobs behind one
  interface, and the app ships the ids the trackers gave it.
- **Decision:** `web::iTracker` is that interface, and `anilist`, `mal` and
  `kitsu` each implement all of it: `search`, `lookUp`/`lookUpMany`
  (AniList answers fifty titles a request; the others go one a second
  through the base class), `startLogin`/`finishLogin`/`current` (each
  site's own OAuth flow behind two calls), `pull` (a user's public list,
  for a one-time move-in with `import --user`) and `push`/`remove`.
  `makeTracker` picks one; no command names a site in its code.
- **One way:** the lists are the truth. Logged in to a site, each saved
  change to a field the sites keep (status, score, progress, volumes,
  rewatches, dates) is pushed to every site where the title is linked
  (`saveAndReport`, undo and redo call `tellSites`). The site's entry is
  overwritten; nothing is read back or merged, so there is nothing to
  reconcile. `rm` leaves the sites alone unless `--remote` is given.
- **Linking:** a title is linked to a site when it has that site's id.
  `add`, `refresh` and `import` keep every id a site gives (AniList gives
  MyAnimeList's; Kitsu's mappings give both others'), so linking is mostly
  automatic; `link`/`unlink` set one by hand. Ids are no step for undo.
- **The provider** is only the default site of `search`, `add`, `refresh`
  and `import --user`, and of `show`'s page link; `--from SITE` overrides it.
- **Known differences** stay behind the interface: only AniList says how
  much of a running show has aired; MyAnimeList answers nothing without
  the program's client id; the score scales differ (AniList's `scoreRaw`
  out of 100, MyAnimeList's 0–10, Kitsu's `ratingTwenty`) and each adapter
  converts from Catalist's 0–10.
- **App ids** are built in (`cmake -DCATALIST_MAL_CLIENT_ID=...` and the
  like, into `version.hpp`) and the same names in the environment override
  them. They are not settings: the provider stays the only one (ADR-011).

## ADR-019: New Data in Tables of Its Own, Logins Outside the Database
- **Context:** Catalist-TUI shares `catalist.db`. It opens only
  `user_version` 1 and writes `entry` with a positional 17-value INSERT,
  so a new `entry` column, or a new version, would break it.
- **Decision:** what this program adds goes in tables the TUI does not
  know, made with `CREATE TABLE IF NOT EXISTS` on write:
  - `entry_id` holds the Kitsu ids, with the MyAnimeList and AniList ids
    the title had when written: the TUI gives a removed title's ref to the
    next one, and a row whose ids no longer match is not read as that
    other title's. It is rewritten with the lists, in their transaction.
  - `outbox` holds the titles the sites are still to be told of (offline,
    a site down, `--offline`, more than ten at once); `sync` sends them.
  The rows of the undo history may have an 18th CSV field, `kitsu_id`,
  which the TUI's reader ignores. `user_version` stays 1.
- **Logins** are kept in `$XDG_STATE_HOME/catalist/accounts` (or
  `~/.local/state/catalist`; with `CATALIST_DATA_DIR`, in that
  directory), mode 600, written beside and renamed into place. Never in
  `catalist.db`: the lists can be copied and shared, the logins not.
  Passwords (Kitsu's) are exchanged for a token and never kept.
- **Verified** against Catalist-TUI 2f589e3 on one file: it opens, edits and
  saves it, keeps the Kitsu ids and the outbox, and each program undoes
  the other's steps. Changes made in the TUI are not queued for the sites;
  `sync --all` sends them.

## ADR-020: Few Dependencies, Little Machinery
- **Context:** the build fetched fmt, SQLite and doctest when the system
  had none, asked pkg-config where fish keeps completions, and linked
  Threads for one writer thread. The code carried a hand-made `result<T>`,
  site errors told apart by their wording, command rows of ten positional
  fields, and four features that each ran through many files: the `cl`
  name, bash completions, the first-run import of 0.3.0's files and
  `CATALIST_TX` groups.
- **Decision:** the build needs CMake, a C++23 compiler and SQLite, and
  doctest for the tests; nothing is downloaded. `std::format` replaced fmt
  and `std::expected` replaced `result<T>`'s own class. `cl` is a shell
  alias away, zsh and fish keep their completions (bash has none), 0.3.0's
  files go through `import`, and `undo -n N` takes several steps back. ctest runs the
  smoke script once, from `sh`.
- **Rule:** a dependency or a feature that threads through several layers
  has to earn it; when the standard library or a shell does the job, it
  does.
