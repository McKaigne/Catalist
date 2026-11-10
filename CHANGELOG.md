# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added
- Two-way sync with MyAnimeList and AniList (Settings → Accounts & Sync,
  `:sync`, `catalist sync`): pulls new and changed entries, sends local edits
  and deletions, removes entries deleted online, and matches MyAnimeList and
  AniList ids so entries reach both lists. A backup is taken before each sync.
- AniList login (paste the token from AniList's PIN page) next to the
  MyAnimeList login; MyAnimeList logins now refresh themselves.
- `catalist login`, `logout`, `sync`, and `import mal|anilist [--user NAME]`
  for importing your own or a public list without syncing.
- MyAnimeList XML export (`catalist export xml`, Settings → Data), accepted
  by both MyAnimeList's and AniList's import pages.

### Changed
- Stats and Settings use the catalogue's cards: Stats has a media and status
  sidebar, an overview, score and start-year charts and a recent-activity
  card; Settings has Themes, Preferences, Accounts & Sync and Data cards.
- The bottom bar shows the MyAnimeList and AniList login state.
- curl now reads request headers and bodies from owner-only temporary files,
  so login tokens never appear on its command line.
- The TUI now has four tabs, always in this order: Anime, Manga, Stats and
  Settings.
- Preferences (theme, mouse, default media and sort, startup tab) are saved
  to `settings.json`, which the app writes itself; `catalist config` reads
  and changes that file and rejects values outside the allowed choices.
- Sync remembers, per entry and list, the values both sides last agreed on
  (in `sync.csv`, which replaces `deleted.csv`) and merges field by field:
  a change made on either side reaches the other, and when the same field
  changed on both sides the local value is kept and the report names the
  entry and the field. The first sync after upgrading treats every entry as
  a first contact; run one last sync on the old build before switching.
- `anime.csv` and `manga.csv` now have the same 16 columns as
  `catalist export csv`; the `dirty`, `external_ids`, `queue_pos` and
  `synced` columns are no longer written. Older files still load and are
  rewritten on the next save.
- Editing notes in the catalogue now shows up in recent activity like other
  edits.

### Fixed
- Dates such as 2024-13-45 or 2023-02-29 are rejected instead of stored.
- `--status` in `ls`, `count`, `add` and `set` rejects unknown values instead
  of treating them as Plan to Watch; an unknown status in a CSV or MAL XML
  file is still loaded as Plan to Watch but logged.
- A remote edit no longer gets lost when both services are connected: it
  reaches the local copy and the other list instead of being overwritten.
- A local edit no longer overwrites an unrelated change made on the list
  since the last sync (for example a score set online while progress was
  raised here).
- Adding a MyAnimeList title whose id equals a local-only entry's id no
  longer merges the two; titles are matched only by MyAnimeList or AniList
  id.
- A list that comes back empty while entries are synced with it no longer
  removes those entries; nothing is changed for that list.
- A failed save (full disk, read-only folder) is reported instead of looking
  like success: CLI commands exit with code 1 and an `io` error, and the TUI
  shows a notice.
- `ls` decides between the terminal table and the piped layout from the
  stream it writes to, and error colour follows stderr.
- `export json` no longer puts a line break before the comma between the
  anime and manga lists.
- The Anime and Manga tabs start in the default sort chosen in Settings.

### Removed
- The episode-count refresh through Jikan, replaced by account sync.
- The Queue, Search, Library, History and Help tabs, with `?` (help), `:help`
  and `a` (add, which opened Search) in the TUI. Add entries with
  `catalist add`. Stored `queue_pos` values are dropped on the next save.
- Kitsu links: the Kitsu badge and stored Kitsu ids, which only the Search
  tab added. Entries with only a Kitsu id lose "open in browser".
- Reading the 7-column and 16-column CSV layouts of development builds
  before 0.2.1; such a file now fails to load with a message naming it
  (open and save it once with Catalist 0.3.0 to convert it).
- Internal leftovers of the removed tabs and the KDL config: the search
  provider classes, a second JSON parser, the rebindable key table and
  unused functions.
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
