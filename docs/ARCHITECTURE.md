# Catalist Architectural Decision Records (ADRs)

## Module map
Everything is in `src/`, each module a `.hpp`/`.cpp` pair:

| Module | Holds |
| :--- | :--- |
| `date`, `result` | `isoDate`; `result<T>`, a value or an error message |
| `entry` | one anime or manga title, its statuses and the rules that change it (progress, status cycle, rating, counts, import merge) |
| `catalogue` | the two lists in memory, kept in `catalist.db`; the import of the old CSV files; undo and redo |
| `store` | a small RAII wrapper over SQLite (`database`, `statement`, `transaction`), the only code that includes `sqlite3.h` |
| `journal` | pure functions over the undo history: what changed between two lists, merging steps, taking one back |
| `malXml` | the MyAnimeList XML format lists are imported and exported in |
| `json`, `anilist` | the web source: title search, and a title's details (both ids, totals, how much is out, the cover) |
| `sync` | `iListSync`, where a future account sync with the provider plugs in (no implementation yet) |
| `updater` | a worker thread for lookups and covers, one request every 2 s, results collected on the interface's tick |
| `system` | everything that touches the OS: files, `runProgram` (curl, img2sixel), `today()`, the terminal's cell size |
| `args`, `refs`, `output` | the command line's POSIX option parser; refs (`a:12`) and title matching; tables, tab-separated lines and JSON Lines |
| `cli` | the command line: one table of commands and their handlers |
| `completion` | the zsh and fish completions and the manual page's COMMANDS, written from the command table |
| `text` | UTF-8 helpers: width, clipping, lower case |
| `tui/app` | `iTab`, `appContext` and the shell (tab bar, bottom bar, global keys) |
| `tui/catalogTab`, `tui/catalogView.cpp` | the Anime and Manga tabs (filters, stats card, list, cover, details): keys and state, and drawing |
| `tui/style` | colours, icons, pill, marquee, key names, `textField` |

`main.cpp` reads the environment, builds the catalogue on the data
directory, then runs the command line, or the TUI when there is no command
and a terminal is attached.

## ADR-001: Comments Say Why, and Label Long Sections
- **Context:** Inline comments explaining step-by-step logic drifted over time.
- **Decision:** Names and small functions say *what* code does. Comments say
  *why*, state invariants and thread-safety contracts, and label the parts
  of longer functions ("The details card: ...") so they read in sections.

## ADR-002: Strong Types and Optionality over Magic Sentinels
- **Context:** Magic values (`-1`, `"0000-00-00"`) caused error-prone checks.
- **Decision:** Use `std::optional<T>` and `result<T>` instead of magic sentinels.

## ADR-003: Decoupled Multi-Provider Subsystem (superseded)
- **Superseded** by ADR-013 and ADR-014: the only web source is AniList,
  in `anilist`.

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
  `readFile`, `writeFileAtomic`, `runProgram`, `httpGet`, `httpPost`,
  `today`, `nowStamp`, `newTxLabel`, `isInteractive`. The database file is
  the other side effect, and `store.cpp` is the only code that includes
  `sqlite3.h`. Where a test must swap one, the caller takes a function
  instead: `anilist::search` takes an `anilist::poster`, and `appContext`
  and `cliContext` carry `today`, `nowStamp` and the poster. The catalogue
  opens its directory directly; tests give it a temporary one.
- **Composition root:** `main.cpp` reads the environment into a
  `dataDirEnv` (`getDefaultDataDir` is pure), builds the catalogue on the
  data directory, and fills the `cliContext` or, through `runTui`, the
  `appContext`. There are no singletons.
- **Processes:** the programs run are curl and img2sixel, with `execvp`
  and an argument list, so nothing passes through a shell.

## ADR-006: Shallow Control Flow
- **Decision:** Function bodies nest at most three levels. Preconditions and
  errors return early; loops skip with `continue`; each distinct subtask is a
  small, named helper (`onSearchKey`, `moveSidebarCursor`, `addPicked`). Declarative layout code (FTXUI element trees) may nest for
  readability.

## ADR-007: Tabs Behind `iTab`, Drawn and Routed by Hand
- **Context:** The TUI was one 2,600-line function whose event handling was a
  single 700-line `CatchEvent` chain.
- **Decision:** Each tab implements `iTab` (`title`, `icon`,
  `render(appContext&)`, `onEvent(const Event&, appContext&)`, `hints()`,
  `onTick`, `mode()`), the program's one interface. `appShell` owns the
  two tabs in a fixed order: it switches tabs (number keys, `Tab`), quits,
  ignores the mouse, turns the redraw timer into `onTick`, and leaves every
  key to the tab while its mode is not normal (`capturesInput()`). It draws
  the tab bar and the bottom bar (mode pill, status, hints). Tabs change the
  catalogue through `appContext::save`, which writes it at once and reports a
  failure in the status line.
- **No component tree:** FTXUI only runs the loop (`Renderer` +
  `CatchEvent`). Text entry is a `textField` drawn by hand, so focus never
  has to be managed.
- **One state per tab:** `catalogTab` knows what the keyboard is doing from
  one `step` (browse, search, type a number, the three add steps, confirm
  delete); `onEvent`, `mode()` and `hints()` are switches over it.

## ADR-008: Naming and Formatting
- **Names:** types, functions, variables and files are camelCase;
  interfaces are `i` + PascalCase (`iTab`); private members end in `_`;
  constants and enumerators are `kPascalCase` (`mediaType::kAnime`,
  `theme::kMauve`). Namespaces stay lower case and macros
  SCREAMING_SNAKE_CASE. Names are descriptive (`widthPx`, `changedTitles`).
- **Formatting:** `.clang-format` (Google-based): tabs for indentation,
  four wide, a 100-column limit, opening braces on the line of their
  statement. Logical operators are written `and`, `or`, `not`.
- **Idioms:** strings are built with `fmt::format`, not by concatenation;
  resources are owned by objects that release them (RAII, e.g. the pipe
  ends in `runProgram`); the standard algorithms replace hand-written loops
  where they say it more plainly. A header is only added when nothing
  already included does the job. Nothing is written to disk but
  `catalist.db` (and SQLite's own journal beside it while it writes).

## ADR-009: One Version, Bumped Only for User-Visible Change
- **Decision:** `VERSION` holds `MAJOR.MINOR.PATCH` and is the only place
  the number is written. CMake reads it into `project(VERSION)`, generates
  `version.hpp` (`kVersionString`), and the program puts it in the man
  page;
  `flake.nix` reads it too, and CI checks `catalist --version` against it.
  PATCH is for fixes and small tweaks, MINOR for new tabs, commands or
  config keys (PATCH resets), MAJOR for big releases; the project stays on
  0.x until the first full-feature release. Internal refactors do not bump.
  Every bump adds a Keep a Changelog entry to `CHANGELOG.md`, is its own
  `release: vX.Y.Z` commit and gets an annotated tag.

## ADR-010: Hermetic Tests That Check Behaviour
- **Decision:** Tests use doctest and are registered with ctest one case at a
  time. The test `main` points `CATALIST_DATA_DIR`, `HOME`,
  `XDG_DATA_HOME` and `XDG_CONFIG_HOME` at a throwaway sandbox and pins `TZ=UTC` before any test
  code runs; each test that touches files gets its own temp directory, and
  a failed write is simulated by a directory where the file should go.
  Everything runs offline: tests pass a fixed date and a `fakeWeb`
  (ADR-005).
- **What a test checks:** the result the user would see: the lists as
  another catalogue reads them back, the order of the rows
  (`catalogTab::shown`), the status line, the exit code and output of a
  command. No test is skipped, expected to fail or switched off by an
  environment variable. `tests/fixtures/csv` holds the CSV layouts still
  imported (those of 0.2.1 and later).
- **From the shells:** `tests/cli/smoke.sh` (POSIX sh, run under dash,
  `bash --posix` and `zsh -f`) and `tests/cli/smoke.fish` drive the built
  program as scripts do, with a stub `curl` first in `PATH` so nothing goes
  online. ctest registers them for each shell it finds, and checks the
  generated completions with `zsh -n` and `fish -n`.
- **Targets:** one library (`catalistLib`, everything but `main.cpp`), the
  program and one test binary (`catalistTests`). CI also runs the suite
  under AddressSanitizer and UBSan.

## ADR-011: No Settings
- **Context:** A settings tab, `settings.json`, themes and `catalist config`
  existed so users could change defaults that hardly anyone changed.
- **Decision:** The defaults are fixed in code (Helix-style): Catppuccin
  Mocha, the list sorted by new releases, the Anime tab first, fixed keys.
  There is no configuration file; an old `settings.json` or `catalist.kdl`
  is ignored. The provider (ADR-014) is the one choice kept, with the
  lists.

## ADR-012: Two-Way List Sync Against a Base (superseded)
- **Superseded** by ADR-013. Sync needed accounts, tokens, a base per title
  and service, request pacing, backups and a job thread: most of the code,
  for a feature a local tracker does not need.

## ADR-013: Local First, One-Way Import
- **Decision:** The catalogue is `catalist.db` and nothing else (ADR-016).
  Lists
  come in with `catalist import` (MyAnimeList XML; AniList lists after
  converting them to that format) and go out with `catalist export`; neither
  touches an account. Re-importing merges by AniList or MyAnimeList id and never
  overwrites the user's score, progress or dates.
- **Network:** the add flow (`a`) runs one AniList search on the UI thread on
  the next tick, after the "Searching" status is drawn; without a
  connection the title is added by hand. Lookups (after every change to a
  title, once a run per title, and once a day for followed titles) and
  covers run on the `updater`'s worker thread, one AniList request every
  2 s; a fetch that failed is tried again after 15 s rather than given up;
  the
  shell folds finished lookups into the catalogue on its tick. Nothing
  about the network is needed to use the lists.
- **Quitting:** the worker's state is shared with its thread, so on exit
  the updater waits only a moment for a request under way; a slow one
  (offline, curl allows eight seconds) finishes alone and wakes nothing.
- **Covers:** FTXUI cannot draw pictures. The cover card is drawn with only
  a note in it, and after each frame the shell writes the sixel picture
  (made by img2sixel, kept in memory) over the card's inside, saving and
  restoring the cursor so FTXUI's next frame lands in place. This is why
  `runTui` runs FTXUI's `Loop` one event at a time.
- **Ids:** an entry's id only numbers it within its list (a new title gets
  the highest id + 1); the command line shows it as the title's ref
  (`a:12`), which can be given again after the title with the highest id
  is removed. Two titles are the same only when
  they share an AniList or a MyAnimeList id. The updater keys a title by its
  AniList id, or by its MyAnimeList id until a lookup reveals the AniList
  one, after which both keys name the same title.

## ADR-014: AniList Only; a Provider, Ready for Sync
- **Context:** Jikan (MyAnimeList's unofficial API) could not be reached
  from some networks, and many users have moved to AniList. AniList's
  public API needs no account and also returns every title's MyAnimeList
  id.
- **Decision:** AniList is the only web source. Entries store both ids. The
  user picks one provider (AniList by default, or MyAnimeList); its ids are
  the ones the interfaces show and accept, and its name is the bottom-left
  pill. The provider lives in the catalogue (`getProvider`, `setProvider`,
  `saveSettings`, the `setting` table of `catalist.db`).
- **Room for sync:** `sync.hpp` declares `iListSync` (`pull`, `push`,
  `remove`) for one site. A sync would be one class per site, made for the
  chosen provider only, matching titles with `idOn(entry, site)`, its token
  kept outside the catalogue, its requests queued on the updater's worker
  thread, its state shown in the provider pill. Nothing implements it yet.

## ADR-015: The Command Line Is for Daily Tracking, Scripts and Pipes
- **Context:** the first list-manager CLI named titles by the provider's
  id, took options anywhere, looked titles up before every change, and
  could not be undone, so it was neither quick to type nor safe to script.
- **One table:** each command is a row in `cli.cpp` (name, aliases,
  summary, details, options with their values and choices, operands and
  their kind, handler, hidden). The row drives `help`, each `--help`, typo
  suggestions, the usage lines, the manual page (`__man` fills
  `doc/catalist.1.in`), and the zsh and fish completions (`completion`).
  The build runs the program to write the page and the scripts, so they
  cannot fall behind it.
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
- **Network:** only `add` and `search` call AniList, synchronously; the CLI
  never starts the updater. `add` fails offline unless `--no-lookup`.
- **Mutating commands:** take the write lock (`beginWrite`), load, resolve
  every operand before changing anything, apply the entry rules
  (`setStatus`, `setScore`, `stepProgress`, `setCount`, as the TUI does),
  save with an undo note, then print one line per changed field.
- **No terminal:** `catalist` with no command and no terminal on stdin or
  stdout prints its usage on stderr and exits with 2.
- **Completion:** the scripts know the commands and options by heart and
  ask the hidden `catalist __complete refs|titles|statuses|fields` for the
  rest. It opens the database read-only (`catalogue::peek`): no lock is
  waited for beyond 100 ms, nothing is created or imported, and any
  failure prints nothing and exits with 0.
- **Testing:** `cliContext` carries the streams, the date, the stamp, the
  AniList poster, `CATALIST_TX` and whether a terminal is attached, so
  tests drive every command with string streams and a fake web
  (ADR-010).

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
- **Undo and redo:** `journal` computes the changes (`diff`), merges the
  commands of one `CATALIST_TX` into one step (`merge`), and takes a step
  back or forward per entry (`transition`): only the fields the step
  changed are checked and set, so web data found since is kept. A title
  changed since stops the step unless forced; a removed title comes back
  under its old id, never over another title. The TUI's saves are steps
  too; its background lookups are not.
- **Old files:** with no `catalist.db`, the CSV files and `settings` of
  0.3.0 and earlier are read from the data directory or, when
  `CATALIST_DATA_DIR` is unset, from `~/.config/catalist`, into a new
  database, once; they are never changed.
