# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Changed
- The TUI now has four tabs, always in this order: Anime, Manga, Stats and
  Settings.
- Preferences (theme, mouse, default media and sort, startup tab) are saved
  to `settings.json`, which the app writes itself; `catalist config` reads
  and changes that file and rejects values outside the allowed choices.
- Metadata sync is a row in Settings → Data (`:sync` still works).

### Fixed
- Dates such as 2024-13-45 or 2023-02-29 are rejected instead of stored.
- `--status` in `ls`, `count`, `add` and `set` rejects unknown values instead
  of treating them as Plan to Watch; an unknown status in a CSV or MAL XML
  file is still loaded as Plan to Watch but logged.
- In the oldest 7-column CSV layout, a numeric title (such as "86") is no
  longer also read as the AniList id.
- `ls` decides between the terminal table and the piped layout from the
  stream it writes to, and error colour follows stderr.
- `export json` no longer puts a line break before the comma between the
  anime and manga lists.
- The Anime and Manga tabs start in the default sort chosen in Settings.

### Removed
- The Queue, Search, Library, History and Help tabs, with `?` (help), `:help`
  and `a` (add, which opened Search) in the TUI. Add entries with
  `catalist add`. Stored `queue_pos` values are kept in the CSV files.
- `catalist.kdl`: custom keybindings, top-bar layout and the KDL parser. Keys
  are fixed; an existing `catalist.kdl` is ignored (set your theme again in
  Settings).

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
