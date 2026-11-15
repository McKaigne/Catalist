# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/).

## [Unreleased]

Catalist is now a small, local-first tracker: two CSV files, two tabs, and
the network only to add titles, keep them current and show their covers.

### Added
- AniList as a second web source beside Jikan, keyed on MyAnimeList ids:
  lookups and covers ask AniList first, the add search asks it when Jikan
  cannot be reached.
- A cover card at the top right of the Anime and Manga tabs, shaped like a
  cover and drawn with sixel graphics through img2sixel (libsixel), fetched
  once the selection rests and kept in memory. The card says why when there
  is no picture, and retries one that could not be fetched. Both side panels
  have the same width, the left headers are in capitals, tracking sits above
  details, and on a narrow or short terminal the table drops its least
  needed columns, the bottom bar its last hints and the right column its
  details card.
- Titles stay current from MyAnimeList: after any change to a title's
  status, rating or progress, and once a day for titles being watched or on
  hold, Catalist looks it up in the background and takes its episode or
  chapter total (a finished series no longer shows `?`) and how many are out.
- New-release and last-updated sorts (`n`, `u`), and a New column with the
  episodes or chapters out but not yet seen. New releases are the default
  order.
- `a` in the Anime and Manga tabs adds a title: type it, look it up on
  MyAnimeList through Jikan, and pick a match. The last row adds the title
  by hand, which also works offline. A title already tracked is selected
  instead of added twice.
- `catalist export anime|manga` prints MyAnimeList XML, which the import
  pages of MyAnimeList and AniList read.
- The bottom-left pill shows the keyboard mode: `NOR`, `SCH` (search), `EDT`
  (typing a number), `ADD` or `DEL` (confirm delete).
- The details card can set the episode or chapter total (for titles added
  by hand), read volumes (manga) and the rewatch or reread count, by one
  with `h`/`l` or by typing a number with `e`/`Enter`; progress can be typed
  too.

### Changed
- The sidebar lists every rating from 10 down to 0.
- Sorting a new column starts in its natural order: rating, progress and
  date from high to low, title and status from A to Z (rating, progress and
  date used to start from the low end). The date column sorts by the date it
  shows.
- Every change is saved at once; a failed save is shown in the status line.
- `catalist import` takes only MyAnimeList XML files, reads them all before
  adding anything, and reports per file how many titles were added.
- New titles are numbered from 1 in each list; an imported title merges
  with a tracked one only when they share a MyAnimeList id.
- `anime.csv` and `manga.csv` have 16 columns (the last two record the web
  checks); files from 0.2.1 to 0.3.0 still load.
- The theme is always Catppuccin Mocha.
- Internal: the code lives in one flat `src/` folder of small modules, with
  one interface (`iTab`); see docs/ARCHITECTURE.md for the module map.
- Internal: tabs for indentation, `and`/`or`/`not`, fmt for building strings
  (a new build dependency) and RAII around the pipes to other programs.

### Fixed
- `+` on a finished title, `-` at zero and `R` at ten no longer stamp
  today's date on the title (a finished title got today as its start and
  finish date).
- The score chart lines up 10 with the other scores.
- A folder where `anime.csv` or `manga.csv` should be no longer crashes
  Catalist on start.
- Dates such as 2024-13-45 or 2023-02-29 are rejected instead of stored.
- A failed save (full disk, read-only folder) is reported instead of looking
  like success: the CLI exits with code 1 and the TUI says so.

### Removed
- The Stats tab. Its totals (days spent, mean score, episodes or chapters,
  rewatches, volumes and a status bar) are in a stats card at the bottom
  left of the Anime and Manga tabs, whose filter rows already count each
  status and rating.
- Notes, and the stored AniList ids, which only sync used; both are dropped
  from the CSV files on the next save, and notes are no longer exported.
- Accounts, logins and sync with MyAnimeList and AniList, with `sync.csv`,
  the backups and the activity history (`history.csv`).
- The Settings tab, `settings.json`, `catalist config`, themes, the command
  palette and the welcome message: the defaults are fixed.
- The Queue, Search, Library, History and Help tabs.
- The CLI commands other than `import` and `export`: `ls`, `count`, `show`,
  `add`, `set`, `rm`, `stats`, `search`, `random`, `completion`, the CSV and
  JSON exports, and `--tab`.
- Undo and redo, visual (batch) selection, multiple sidebar filters, mouse
  support, opening a title in the browser and copying it, and the log file.
- `catalist.kdl` and its parser; an existing file is ignored, as are the
  old `settings.json`, `history.csv`, `sync.csv`, `catalist.log`,
  `mal_auth.json`, `anilist_auth.json` and `backups/`, which can be deleted.
- Kitsu links and stored Kitsu ids, and stored `queue_pos` values (dropped
  on the next save).
- Reading the 7-column and 16-column CSV layouts of development builds
  before 0.2.1; such a file fails to load with a message naming it (open and
  save it once with Catalist 0.3.0 to convert it). Files from 0.2.1 to 0.3.0
  load, `0000-00-00` dates included.

## [0.3.0] - 2026-11-10

### Added
- Queue tab: Watching and Plan to Watch entries in your own order; reorder
  with `J`/`K`. The order is stored in a new `queue_pos` CSV column.
- Search tab: full-screen online search with a MyAnimeList / AniList / Kitsu
  switcher, provider badges and an anime/manga toggle; `Enter` adds the result
  and keeps the results open. `a` in the catalogue opens it.
- Library tab: browse by start year, then status, then entries.
- History tab: an activity heatmap and a day-by-day log of updates.
- Stats tab: score histogram, entries-per-start-year chart and an anime/manga
  toggle.
- Settings: live theme preview, a Data section (backup list with restore,
  MyAnimeList XML import, CSV export, MyAnimeList login and logout) and a
  startup tab preference.
- Help tab with the current keybindings, a one-time welcome on first run and
  the `:about` and `:tab <id>` commands.
- `startup-tab` in `catalist.kdl` and the `catalist --tab <id>` flag.
- Number keys follow the tab order, and the number is shown in the top bar.

### Changed
- The Overview tab is now Stats and the About tab is now Help.
- History timestamps are stored as `YYYY-MM-DD HH:MM`; older entries still
  load. Up to 5000 history records are kept (was 100).
- Sort direction is a single toggle in Settings; default media is anime or
  manga.
- Older `catalist.kdl` files that list the removed tabs get the new tab set
  automatically.

### Removed
- The About and Sync tabs (metadata sync stays available as `:sync`).
- The online search pop-up in the catalogue, replaced by the Search tab.
- The "overview" default-media option.

## [0.2.1] - 2026-11-10

### Fixed
- Security: the MyAnimeList OAuth authorization code was inserted into a
  shell command without escaping. Token exchange now goes through the HTTP
  client and the code is form-encoded, so a crafted code can neither run
  commands nor inject form fields.
- Security: the OAuth redirect listener bound to every network interface; it
  now listens on 127.0.0.1 only.
- Opening links and copying to the clipboard no longer pass URLs or titles
  through a shell command line.
- AniList search results reported the MyAnimeList id as the AniList id, so
  AniList links pointed at the wrong page.

## [0.2.0] - 2026-11-04

### Added
- Terminal UI with Anime, Manga, Overview, Sync, Settings and About tabs,
  floating detail card, command palette, visual multi-selection and undo/redo.
- CLI query commands: `ls`, `count`, `show`, `random`, `stats`, with table,
  `--plain`, `--ids`, `--csv` and `--json` output.
- CLI mutation commands: `add`, `set` (absolute and stepped progress), `rm`.
- `export` (JSON, CSV), MyAnimeList XML `import`, and online `search`.
- Provider registry with MyAnimeList, AniList and Kitsu.
- MyAnimeList OAuth login and metadata sync.
- KDL configuration (`catalist.kdl`), themes, keybinds and shell completions.
- Atomic CSV storage with automatic backup snapshots.
