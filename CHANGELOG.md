# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/).

## [Unreleased]

Catalist is now a small, local-first tracker for the command line: one
SQLite file, commands made for daily tracking, scripts and pipes with undo
and redo, and the network only to add and search for titles. The
full-screen terminal interface is now a separate program, Catalist-TUI,
which keeps its lists in the same file.

### Added
- Undo and redo. Every command that changes the lists is one step for
  `catalist undo`; `catalist redo` makes it again, `catalist log` lists the
  last 100 steps. Commands run with the same `CATALIST_TX` (from
  `catalist tx`) are one step. A title changed since is not overwritten
  unless `-f` is given.
- `cl`, a link to `catalist` installed beside it, and completions for zsh
  (`_catalist`) and fish (`catalist.fish`, `cl.fish`), installed by CMake
  and Nix, that complete commands, options, values and your titles by ref
  or by name. The manual page lists every command.
- AniList is the one web source (search, totals, aired episodes), replacing
  Jikan, which some networks cannot reach at all. Entries carry both the
  AniList and the MyAnimeList id, read from 0.3's files too.
- A provider, AniList (default) or MyAnimeList: `show` links to a title's
  page there, and it is where an account sync will plug in (`iListSync`,
  declared, not yet implemented). `catalist provider anilist|mal` chooses
  it.
- Commands that change the tracking of titles: `+` and `-`, `set`, and
  `done`, `hold`, `drop`, `plan` and `resume`.

### Changed
- The command line is redesigned for daily tracking, scripts and pipes:
  `ls`, `show`, `stats`, `+` (`inc`), `-` (`dec`), `set`, `done`, `hold`,
  `drop`, `plan`, `resume`, `rm`, `add`, `search`, `import`, `export`,
  `undo`, `redo`, `log`, `tx`, `provider`, `completion` and `help`, all from
  one table of commands that also writes the help, the manual page and the
  completions.
  - Titles are named by a ref, the first column of `ls` (`a:12`, `m:3`), or
    by part of the title; `--all` takes every match, and a lone `-` reads
    refs from standard input, so `catalist ls -q | catalist + -` works.
  - POSIX options (grouped, `-n3`, `--name=value`, `--`), `--help` on every
    command, suggestions for typos. A table in colour at a terminal
    (`NO_COLOR` respected); into a pipe, one tab-separated line a title,
    `--json` for JSON Lines and `--header` for column names. Messages and
    prompts on stderr; exit codes 0, 1 and 2; a pipe closed early, as by
    `head`, ends the command quietly. `-q` prints nothing at all.
  - Only `add` and `search` go online; `add --no-lookup` adds a title by
    hand, offline.
- `catalist export anime|manga` prints MyAnimeList XML, which the import
  pages of MyAnimeList and AniList read; `--format=csv` and `--format=json`
  print every title with the columns of `ls`.
- The lists, the provider and the undo history are kept in one SQLite file,
  `catalist.db`, in `~/.local/share/catalist` (or `$XDG_DATA_HOME/catalist`,
  or `CATALIST_DATA_DIR`), instead of `anime.csv`, `manga.csv` and
  `settings` in `~/.config/catalist`. Those files are read into the new one
  the first time, with a note on stderr, and left as they were. Each change
  is one transaction, and two programs at once wait for each other. SQLite
  is a new build dependency (downloaded by CMake when the system has none).
- `catalist` with no command prints its usage and exits with 2.
- `catalist import` takes only MyAnimeList XML files (`-` for standard
  input), reads them all before adding anything, reports per file how many
  titles were added, and is one step for undo.
- New titles are numbered from 1 in each list; an imported title merges
  with a tracked one only when they share an AniList or MyAnimeList id.
- Internal: the sources are in folders by layer (`core`, `storage`, `web`,
  `cli`), the commands in one file per group, the tests in matching
  folders; fmt builds the strings (a new build dependency); see
  docs/ARCHITECTURE.md.

### Fixed
- `+` on a finished title and `-` at zero no longer stamp today's date on
  the title (a finished title got today as its start and finish date).
- A folder where `anime.csv` or `manga.csv` should be no longer crashes
  Catalist.
- Dates such as 2024-13-45 or 2023-02-29 are rejected instead of stored.
- A failed save (full disk, read-only folder) is reported with exit code 1
  instead of looking like success.

### Removed
- The full-screen terminal interface (the Anime and Manga tabs, their keys,
  the cover card drawn with img2sixel, and the background checks that kept
  totals current); FTXUI and libsixel are no longer needed. It lives on as
  Catalist-TUI.
- Notes: they are not read from the old CSV files, and no longer exported.
- Accounts, logins and sync with MyAnimeList and AniList, with `sync.csv`,
  the backups and the activity history (`history.csv`).
- The Settings tab, `settings.json`, `catalist config`, themes, the command
  palette and the welcome message: the defaults are fixed.
- The CLI commands `count` and `random`, and `--tab`.
- `catalist.kdl` and its parser; an existing file is ignored, as are the
  old `settings.json`, `history.csv`, `sync.csv`, `catalist.log`,
  `mal_auth.json`, `anilist_auth.json` and `backups/`, which can be deleted.
- Kitsu links and stored Kitsu ids, and stored `queue_pos` values.
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
