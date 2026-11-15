# Catalist Architectural Decision Records (ADRs)

## Module map
Everything is in `src/`, each module a `.hpp`/`.cpp` pair:

| Module | Holds |
| :--- | :--- |
| `date`, `result` | `isoDate`; `result<T>`, a value or an error message |
| `entry` | one anime or manga title, its statuses and the rules that change it (progress, status cycle, rating, counts, import merge) |
| `catalogue` | the two lists in memory and their CSV files |
| `malXml` | the MyAnimeList XML format lists are imported and exported in |
| `json`, `anilist` | the web source: title search, and a title's details (both ids, totals, how much is out, the cover) |
| `sync` | `iListSync`, where a future account sync with the provider plugs in (no implementation yet) |
| `updater` | a worker thread for lookups and covers, one request every 2 s, results collected on the interface's tick |
| `system` | everything that touches the OS: files, `runProgram` (curl, img2sixel), `today()`, the terminal's cell size |
| `cli` | the command line: `ls`, `show`, `add`, `set`, `next`, `rm`, `search`, `provider`, `import`, `export` |
| `tui/app` | `iTab`, `appContext` and the shell (tab bar, bottom bar, global keys) |
| `tui/catalogTab`, `tui/catalogView.cpp` | the Anime and Manga tabs (filters, stats card, list, cover, details): keys and state, and drawing |
| `tui/style` | colours, icons, pill, marquee, key names, `textField` |

`main.cpp` loads the catalogue, then runs the command line or the TUI.

## ADR-001: Comments Say Why, and Label Long Sections
- **Context:** Inline comments explaining step-by-step logic drifted over time.
- **Decision:** Names and small functions say *what* code does. Comments say
  *why*, state invariants and thread-safety contracts, and label the parts
  of longer functions ("The details card: ...") so they read in sections.

## ADR-002: Strong Types and Optionality over Magic Sentinels
- **Context:** Magic values (`-1`, `"0000-00-00"`) caused error-prone checks.
- **Decision:** Use `std::optional<T>` and `result<T>` instead of magic sentinels.

## ADR-003: Decoupled Multi-Provider Subsystem (superseded)
- **Superseded** by ADR-013: the only online lookup left is the Jikan title
  search in `jikan`.

## ADR-004: Explicit Unix Filter and Standard Stream Discipline
- **Context:** Terminal tools must integrate smoothly into shell pipelines.
- **Decision:** `catalist export` writes only the XML to `stdout`; warnings
  and errors go to `stderr`, and the exit code says what happened (0 done,
  1 failed, 2 wrong command line).

## ADR-005: Side Effects in One Place, Passed as Functions Where Tests Swap Them
- **Context:** Code that reads files, the clock or the network cannot be
  tested without a home directory, a fixed date or a connection. Interfaces
  with a real class and a fake for each (`iFileSystem`, `iHttpClient`,
  `iClock`) did the job, but each wrapped only one or two calls.
- **Decision:** `system.cpp` is the only code that touches file streams,
  `popen` or the system clock, as plain functions: `readFile`,
  `writeFileAtomic`, `httpGet`, `today`. Where a test must swap one, the
  caller takes a function instead: `jikan::search` and `appContext` take a
  `jikan::fetcher`, and `appContext` a `today` function. The catalogue
  reads and writes its directory directly; tests give it a temporary one.
- **Composition root:** `main.cpp` builds the catalogue on the data
  directory; `runTui` puts `today` and `httpGet` into the `appContext`.
  There are no singletons.
- **Processes:** the only command run is curl, with the URL quoted by
  `shellQuote`.

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
  already included does the job. Nothing is written to disk but the two
  catalogue files.

## ADR-009: One Version, Bumped Only for User-Visible Change
- **Decision:** `VERSION` holds `MAJOR.MINOR.PATCH` and is the only place
  the number is written. CMake reads it into `project(VERSION)`, generates
  `version.hpp` (`kVersionString`) and the man page;
  `flake.nix` reads it too, and CI checks `catalist --version` against it.
  PATCH is for fixes and small tweaks, MINOR for new tabs, commands or
  config keys (PATCH resets), MAJOR for big releases; the project stays on
  0.x until the first full-feature release. Internal refactors do not bump.
  Every bump adds a Keep a Changelog entry to `CHANGELOG.md`, is its own
  `release: vX.Y.Z` commit and gets an annotated tag.

## ADR-010: Hermetic Tests That Check Behaviour
- **Decision:** Tests use doctest and are registered with ctest one case at a
  time. The test `main` points `CATALIST_DATA_DIR`, `HOME` and
  `XDG_CONFIG_HOME` at a throwaway sandbox and pins `TZ=UTC` before any test
  code runs; each test that touches files gets its own temp directory, and
  a failed write is simulated by a directory where the file should go.
  Everything runs offline: tests pass a fixed date and a `fakeWeb`
  (ADR-005).
- **What a test checks:** the result the user would see: the saved CSV text,
  the order of the rows (`catalogTab::shown`), the status line, the exit code
  and output of a command. No test is skipped, expected to fail or switched
  off by an environment variable. `tests/fixtures/csv` holds the CSV layouts
  still read (those of 0.2.1 and later).
- **Targets:** one library (`catalistLib`, everything but `main.cpp`), the
  program and one test binary (`catalistTests`). CI also runs the suite
  under AddressSanitizer and UBSan.

## ADR-011: No Settings
- **Context:** A settings tab, `settings.json`, themes and `catalist config`
  existed so users could change defaults that hardly anyone changed.
- **Decision:** The defaults are fixed in code (Helix-style): Catppuccin
  Mocha, the list sorted by rating, the Anime tab first, fixed keys. There is
  no configuration file; an old `settings.json` or `catalist.kdl` is ignored.

## ADR-012: Two-Way List Sync Against a Base (superseded)
- **Superseded** by ADR-013. Sync needed accounts, tokens, a base per title
  and service, request pacing, backups and a job thread: most of the code,
  for a feature a local tracker does not need.

## ADR-013: Local First, One-Way Import
- **Decision:** The catalogue is the two CSV files and nothing else. Lists
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
  the highest id + 1) and is never shown; two titles are the same only when
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
  `saveSettings`, the `settings` file).
- **Room for sync:** `sync.hpp` declares `iListSync` (`pull`, `push`,
  `remove`) for one site. A sync would be one class per site, made for the
  chosen provider only, matching titles with `idOn(entry, site)`, its token
  kept outside the catalogue, its requests queued on the updater's worker
  thread, its state shown in the provider pill. Nothing implements it yet.

## ADR-015: A Command Line for Managing the Lists
- **Context:** the CLI could only import and export; adding or changing a
  title meant opening the TUI.
- **Decision:** subcommands in the style of git and cargo, defined as a
  table in `cli.cpp` (name, aliases, usage, options, handler) that also
  drives `help`, per-command `--help` and typo suggestions. A small parser
  takes `--long value`, `--long=value`, `-s value` and `--`; anything like
  `-3` is a word, so `--progress -3` works.
- **Naming titles:** by the provider's id, or by words from the title
  (exact title first). Ambiguity is never resolved by guessing: a terminal
  is asked, a script gets the list and exit code 1.
- **Same rules as the TUI:** edits go through the entry rules
  (`setStatus`, `setScore`, `stepProgress`, `setCount`), and a title not
  checked today is looked up first. The CLI runs requests synchronously; it
  never starts the updater thread.
- **Testing:** `cliContext` carries the streams, the date, the AniList
  poster and whether a terminal is attached, so tests drive every command
  with string streams and a fake web.

