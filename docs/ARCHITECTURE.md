# Catalist Architectural Decision Records (ADRs)

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
  search in `net/jikan`.

## ADR-004: Explicit Unix Filter and Standard Stream Discipline
- **Context:** Terminal tools must integrate smoothly into shell pipelines.
- **Decision:** `catalist export` writes only the XML to `stdout`; warnings
  and errors go to `stderr`, and the exit code says what happened (0 done,
  1 failed, 2 wrong command line).

## ADR-005: Dependency Injection at Every Side-Effect Boundary
- **Context:** Domain code that reads files, the clock or the network cannot
  be tested without a home directory, a fixed date or a connection.
- **Decision:** Every side effect sits behind a small pure virtual
  interface: `iFileSystem` (read, atomic write), `iHttpClient` (one GET) and
  `iClock`. The real implementations (`realFileSystem`, `curlHttpClient`,
  `realClock`) are the only code that touches file streams, `popen` or
  `system_clock::now`. The fakes (`memoryFileSystem`, `mockHttpClient`,
  `fixedClock`) live in `tests/support/fakes.hpp`, not in the program.
- **Composition root:** `main.cpp` builds the real services and passes them
  by reference (`database(fileSystem, dir)`, `runCli(...)`,
  `runTui(db, clock, http)` → `appContext`). There are no singletons.
- **Processes:** the only command run is curl, with the URL quoted by
  `shell::escapeShellArg`.

## ADR-006: Shallow Control Flow
- **Decision:** Function bodies nest at most three levels. Preconditions and
  errors return early; loops skip with `continue`; each distinct subtask is a
  small, named helper (`handleSearchTyping`, `moveSidebarCursor`,
  `addPicked`). Declarative layout code (FTXUI element trees) may nest for
  readability.

## ADR-007: Tabs Behind `iTab`
- **Context:** The TUI was one 2,600-line function whose event handling was a
  single 700-line `CatchEvent` chain.
- **Decision:** Each tab implements `iTab` (`title`, `icon`,
  `render(appContext&)`, `onEvent(const Event&, appContext&)`, `hints()`,
  `onTick`, `mode()`). `appShell` owns the three tabs in a fixed order: it
  switches tabs (number keys, `Tab`), quits, ignores the mouse, turns the
  redraw timer into `onTick`, and yields every key while a tab
  `capturesInput()`. It draws the tab bar and the bottom bar (mode pill,
  status, hints). Shared pieces live in `tui/widgets` (pill, card, marquee)
  and `tui/keys`. Tabs change the catalogue through `appContext::save`, which
  writes it at once and reports a failure in the status line.
- **Layering:** `catalistCore` (core, platform, model, storage, net, cli)
  never includes FTXUI; `catalistTui` builds on it.

## ADR-008: camelCase Everywhere
- **Decision:** Types, functions, variables and files are camelCase;
  interfaces are `i` + PascalCase (`iTab`, `iHttpClient`); private members end
  in `_`; constants and enumerators are `kPascalCase`
  (`mediaType::kAnime`, `theme::kMauve`). Namespaces stay lower case and macros
  SCREAMING_SNAKE_CASE. When a type and a variable would share a name, the
  variable is renamed (`mediaEntry`, `shown`).

## ADR-009: One Version, Bumped Only for User-Visible Change
- **Decision:** `VERSION` holds `MAJOR.MINOR.PATCH` and is the only place
  the number is written. CMake reads it into `project(VERSION)`, generates
  `catalist/core/version.hpp` (`kVersionString` and friends) and the man page;
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
  code runs; each test that touches real files gets its own temp directory.
  Everything runs offline and deterministically with the fakes from ADR-005.
- **What a test checks:** the result the user would see: the saved CSV text,
  the order of the rows (`catalogTab::shown`), the status line, the exit code
  and output of a command. No test is skipped, expected to fail or switched
  off by an environment variable. `tests/fixtures/csv` holds the CSV layouts
  still read (those of 0.2.1 and later).
- **Targets:** `catalistTests` links only `catalistCore`; tab, shell and
  widget tests need FTXUI and live in `catalistTuiTests`. CI also runs the
  suite under AddressSanitizer and UBSan.

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
