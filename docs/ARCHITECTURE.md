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
- **Context:** Remote search routines were split across ad-hoc curl routines.
- **Decision:** Unify external providers behind the `iProvider` interface,
  managed by `providerRegistry`.
- **Superseded:** Once the Search tab was removed, nothing called a
  provider's search; only a badge and a web address per service were left.
  Those are now a constant table in `model/serviceLinks`, and `catalist
  search` keeps using `net/remoteSearch`.

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

## ADR-012: Two-Way List Sync Against a Base
- **Context:** Catalist should stay usable offline and in the CLI while
  keeping MyAnimeList and AniList lists current in both directions. A single
  "edited locally" flag could not say which service had not seen a change,
  and nothing remembered what the two sides last agreed on, so remote edits
  were lost or overwritten.
- **Decision:** Each service implements `iListService` (fetch the whole list,
  save one row, delete one row). For every title and service, `sync.csv`
  keeps the **base**: the status, score, progress, volumes, rewatches, dates
  and notes both sides last agreed on. Whichever side differs from the base
  is the side that changed. For one service and media type, `listSync`
  fetches the list and settles every service id seen locally, in the bases
  or in the list:

  | Local | Base | Remote | Action |
  | --- | --- | --- | --- |
  | yes | yes | yes | Merge field by field; push and update as needed |
  | yes | no | yes | First contact: if they differ, the later update date wins, a tie goes to local |
  | yes | yes | no | Deleted there: remove locally, or push back (and report) if edited here |
  | no | yes | yes | Deleted here: delete on the service |
  | yes | no | no | Never sent: push |
  | no | no | yes | New on the service: add locally |
  | no | yes | no | Gone from both: forget the base |

  A field equal to its base takes the other side's value; when both sides
  changed it and disagree, the local value is kept and the report names the
  title and the field. After an action succeeds the base becomes the agreed
  values (or is erased); after a failed push it stays, so the next sync
  reaches the same decision and no retry list is needed. A failed fetch, or
  an empty list while bases exist, changes nothing for that service.
  Totals and missing ids come from the service; the local title is kept.
  AniList's public `idMal` lookups fill in missing ids so entries cross
  between the services, and a second round runs when a pass changed the
  catalogue, so a change pulled from the later service reaches the earlier
  one. A backup snapshot is written before every run. Requests are paced
  through an injected function, so tests run instantly.
- **Consequences:** A sync costs one list fetch per service and media type
  plus one request per change. `saveAll` writes the catalogue first and
  `sync.csv` last; a crash between the two leaves old bases, which the next
  sync repairs. Undoing a delete needs no special code: the entry is local
  again, its base is still there, and the first row applies. The TUI runs
  sync on the job thread and does not draw the catalogue until it ends,
  because the worker writes the database.
