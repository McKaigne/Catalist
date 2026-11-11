# Catalist Architectural Decision Records (ADRs)

## Module map
Everything is in `src/`, each module a `.hpp`/`.cpp` pair:

| Module | Holds |
| :--- | :--- |
| `date`, `result` | `isoDate`; `result<T>`, a value or an error message |
| `entry` | one anime or manga title, its statuses and the rules that change it (progress, status cycle, rating, counts, import merge) |
| `catalogue` | the two lists in memory and their CSV files |
| `malXml` | the MyAnimeList XML format lists are imported and exported in |
| `json`, `jikan` | the title search on Jikan, the only network use |
| `system` | everything that touches the OS: files, `httpGet` (curl), `today()` |
| `cli` | `catalist import` and `catalist export` |
| `tui/app` | `iTab`, `appContext` and the shell (tab bar, bottom bar, global keys) |
| `tui/catalogTab`, `tui/catalogView.cpp` | the Anime and Manga tabs: keys and state, and drawing |
| `tui/statsTab` | the Stats tab |
| `tui/style` | colours, icons, pill, card, marquee, key names, `textField` |

`main.cpp` loads the catalogue, then runs the command line or the TUI.

## ADR-001: Self-Documenting Code over Inline Code Comments
- **Context:** Inline comments explaining step-by-step logic drifted over time.
- **Decision:** Remove comments explaining *what* code does. Express intent
  through semantic function names and boolean predicates. Reserve comments
  strictly for *why*, invariants, and thread-safety contracts.

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
  three tabs in a fixed order: it switches tabs (number keys, `Tab`), quits,
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

## ADR-008: camelCase Everywhere
- **Decision:** Types, functions, variables and files are camelCase;
  interfaces are `i` + PascalCase (`iTab`); private members end
  in `_`; constants and enumerators are `kPascalCase`
  (`mediaType::kAnime`, `theme::kMauve`). Namespaces stay lower case and macros
  SCREAMING_SNAKE_CASE. When a type and a variable would share a name, the
  variable is renamed (`mediaEntry`, `shown`).

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
  touches an account. Re-importing merges by MyAnimeList id and never
  overwrites the user's score, progress or dates.
- **Network:** only the add flow (`a`) uses it, for one Jikan title search;
  without a connection the title is added by hand. The search runs on the
  UI thread on the next redraw tick, after the "Searching" status is drawn,
  and curl gives up after 8 seconds, so no worker thread is needed.
- **Ids:** an entry's id only numbers it within its list (a new title gets
  the highest id + 1); two titles are the same only when they share a
  MyAnimeList id.
