# Catalist Architectural Decision Records (ADRs)

## ADR-001: Self-Documenting Code over Inline Code Comments
- **Context:** Inline comments explaining step-by-step logic drifted over time.
- **Decision:** Remove comments explaining *what* code does. Express intent
  through semantic function names and boolean predicates. Reserve comments
  strictly for *why*, invariants, and thread-safety contracts.

## ADR-002: Strong Types and Optionality over Magic Sentinels
- **Context:** Magic values (`-1`, `"0000-00-00"`) caused error-prone checks.
- **Decision:** Use `std::optional<T>` and `result<T>` instead of magic sentinels.

## ADR-003: Decoupled Multi-Provider Subsystem
- **Context:** Remote search routines were split across ad-hoc curl routines.
- **Decision:** Unify external providers behind the `iProvider` interface,
  managed by `providerRegistry`.

## ADR-004: Explicit Unix Filter and Standard Stream Discipline
- **Context:** Terminal tools must integrate smoothly into shell pipelines.
- **Decision:** Formatted tables go to `stdout` only on interactive TTYs;
  `--plain`, `--ids`, and `--json` produce clean machine-readable streams;
  all diagnostic and error messages route strictly to `stderr`.

## ADR-005: Dependency Injection at Every Side-Effect Boundary
- **Context:** Domain code called `curl` through `popen`, read files with
  `std::ifstream`, read the system clock and reached for singletons, so none
  of it could be tested without a network, a home directory or a fixed date.
- **Decision:** Every side effect sits behind a pure virtual interface:
  `iHttpClient`, `iFileSystem`, `iProcessLauncher` and `iClock`, plus
  `iAuthCallbackServer` for the OAuth redirect. Each has a real and a fake
  implementation (`curlHttpClient`/`mockHttpClient`,
  `realFileSystem`/`memoryFileSystem`, `realProcessLauncher`/
  `fakeProcessLauncher`, `realClock`/`fixedClock`). `popen`, `system`,
  `fopen`, file streams and `system_clock::now` appear only inside the real
  implementations under `platform/` and `core/clock`.
- **Composition root:** `main.cpp` builds the real services and passes them
  by reference (`cli::commandContext`, `tuiServices` → `appContext`,
  `database(fileSystem, clock, dir)`, `malAuth(...)`). The only remaining
  process-wide object is the logger behind the `LOG_*` macros; it is
  initialised in `main.cpp` with the injected file system and clock and drops
  messages before that.
- **Processes:** commands are argv vectors, never shell strings
  (`shell::escapeShellArg` quotes each word when a shell is unavoidable).

## ADR-006: Shallow Control Flow
- **Decision:** Function bodies nest at most three levels. Preconditions and
  errors return early; loops skip with `continue`; each distinct subtask is a
  small, named helper (`handleSearchTyping`, `isRowFilterActive`,
  `applyValue`). Declarative layout code (FTXUI element trees) may nest for
  readability.

## ADR-007: Tabs Behind `iTab`
- **Context:** The TUI was one 2,600-line function whose event handling was a
  single 700-line `CatchEvent` chain.
- **Decision:** Each tab implements `iTab` (`id`, `title`, `icon`,
  `render(appContext&)`, `onEvent(const Event&, appContext&)`, `hints()`)
  and is its own FTXUI component inside `Container::Tab`. The app shell owns
  the four tabs in a fixed order, and the number keys follow it. The shell's
  global handler
  only switches tabs, opens the command palette and quits, and yields while
  a tab `capturesInput()`. Shared pieces live in `tui/widgets` (pill,
  marquee, notifier, commandPalette), `tui/keys` (the fixed key map and
  movement predicates) and `tui/actions` (browser, clipboard, import, backup).
- **Layering:** `catalistCore` (core, platform, model, storage, net, settings)
  never includes FTXUI; `catalistCli` and `catalistTui` build on it.

## ADR-008: camelCase Everywhere
- **Decision:** Types, functions, variables and files are camelCase;
  interfaces are `i` + PascalCase (`iTab`, `iHttpClient`); private members end
  in `_`; constants and enumerators are `kPascalCase`
  (`mediaType::kAnime`, `theme::kBase`). Namespaces stay lower case and macros
  SCREAMING_SNAKE_CASE. When a type and a variable would share a name, the
  variable is renamed (`mediaEntry`, `valueType`, `notices`).

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

## ADR-010: Hermetic, Characterization-First Tests
- **Decision:** Tests use doctest and are registered with ctest one case at a
  time. The test `main` points `CATALIST_DATA_DIR`, `HOME` and
  `XDG_CONFIG_HOME` at a throwaway sandbox and pins `TZ=UTC` before any test
  code runs; each file-touching test gets its own temp directory. Business
  logic is tested offline and deterministically with the fakes from ADR-005.
- **Characterization:** before refactoring, golden files in
  `tests/fixtures/golden` recorded the CLI output and `tests/fixtures/csv`
  every historical CSV layout; refactors must keep them passing.
  `CATALIST_UPDATE_GOLDEN=1` regenerates goldens deliberately.
- **Targets:** `catalistTests` links only `catalistCore` and `catalistCli`;
  tab, widget and theme tests need FTXUI and live in `catalistTuiTests`.
  CI also runs the suite under AddressSanitizer and UBSan.

## ADR-011: Preferences in settings.json, No Config Language
- **Context:** `catalist.kdl` let users rebind keys and lay out the top bar,
  which needed a KDL parser and serializer, legacy-file upgrades and a key
  table that every tab had to consult.
- **Decision:** The handful of preferences the Settings tab changes live in
  `settings.json`, which Catalist owns and rewrites atomically. Unknown keys,
  wrong types and values outside the allowed choices fall back to the
  defaults. Keys and tab order are fixed in code (`tui/keys`, the app shell).
- **Consequences:** No user-facing config format to keep compatible; old
  `catalist.kdl` files are ignored.
