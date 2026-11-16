# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/).

## [Unreleased]

Catalist is now a small, local-first tracker for the command line: one
SQLite file, commands made for daily tracking, scripts and pipes with undo
and redo, and AniList, MyAnimeList and Kitsu as equal sites to find titles
on and, logged in, to tell what changes, one way, as Mihon tells its
trackers. The full-screen terminal interface is now a separate program,
Catalist-TUI, which keeps its lists in the same file.

### Added
- Tracking, as Mihon does it: `catalist login anilist|mal|kitsu` (each
  site's own way in; tokens kept apart from the lists, mode 600, passwords
  never), `logout` and `accounts`. Logged in, each change to a title's
  status, score, progress, volumes, rewatches or dates is sent to every
  such site where the title is linked; the lists are the truth, and
  nothing is read back. What could not be sent waits for `catalist sync`
  (`--all` sends every linked title, `--forget` drops what waits);
  `--offline` and `CATALIST_OFFLINE` keep everything for it. `link`,
  `unlink` and `ls --unlinked SITE` manage which titles a site hears of;
  `rm --remote` takes titles off the sites as well.
- MyAnimeList and Kitsu beside AniList, behind one interface: `search`,
  `add`, `refresh` and `import --user` ask your provider, or the site
  `--from` names. Entries keep their Kitsu id too. MyAnimeList's API needs
  the client id it gave Catalist (`-DCATALIST_MAL_CLIENT_ID`, or the same
  name in the environment); logging in to AniList and Kitsu wants theirs.
- `catalist refresh [REF...]`: asks a site for totals and how much is out
  (the `new` column) of the titles you follow, fifty a request on AniList.
- `catalist import --user NAME`: a user's public lists on a site, moved in
  the way a file is.
- `catalist export --format=json` is a full backup, every id included, and
  `import` reads it back.
- `catalist` alone at a terminal lists what you are watching, new episodes
  first; `ls`, `search` and `add` take a title of several words without
  quotes; a REF that matches nothing names the title it probably meant.
- `ls -s watching,on-hold`, `set start=today`, `add --score N --progress N`.
- `--color=auto|always|never` and `-t/--table` before the command (also
  `CLICOLOR_FORCE` and `COLUMNS`).
- Undo and redo. Every command that changes the lists is one step for
  `catalist undo`; `catalist redo` makes it again, `catalist log` lists the
  last 100 steps, and `undo -n N` takes back several. A title changed
  since is not overwritten unless `-f` is given.
- Completions for zsh (`_catalist`) and fish (`catalist.fish`), installed
  by CMake and Nix, that complete commands, options, values and your
  titles by ref or by name. The manual page lists every command.
- Jikan, which some networks cannot reach at all, is gone; entries carry
  the AniList and the MyAnimeList id, read from 0.3's files too.
- A provider, AniList (default), MyAnimeList or Kitsu: the default site of
  `search`, `add`, `refresh` and `import --user`, and the page `show` links
  to. `catalist provider anilist|mal|kitsu` chooses it.
- Commands that change the tracking of titles: `+` and `-`, `set`, and
  `done`, `hold`, `drop`, `plan` and `resume`.

### Changed
- The command line is redesigned for daily tracking, scripts and pipes:
  `ls`, `show`, `stats`, `+` (`inc`), `-` (`dec`), `set`, `done`, `hold`,
  `drop`, `plan`, `resume`, `rm`, `add`, `search`, `import`, `export`,
  `undo`, `redo`, `log`, `provider`, `completion` and `help`, all from
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
  - Only `add`, `search`, `refresh`, `import --user`, `login`, `sync` and
    the changes sent to the sites you are logged in to go online; `add
    --no-lookup` adds a title by hand, offline.
- `catalist export anime|manga` prints MyAnimeList XML, which the import
  pages of MyAnimeList, AniList and Kitsu read; `--format=csv` and
  `--format=json` print every title with the columns of `show`, both lists
  when none is named.
- The lists, the provider and the undo history are kept in one SQLite file,
  `catalist.db`, in `~/.local/share/catalist` (or `$XDG_DATA_HOME/catalist`,
  or `CATALIST_DATA_DIR`), instead of `anime.csv`, `manga.csv` and
  `settings` in `~/.config/catalist`. Move in with `catalist import
  ~/.config/catalist/anime.csv ~/.config/catalist/manga.csv`, which leaves
  them as they were (choose the provider again with `catalist provider`).
  Each change is one transaction, and two programs at once wait for each
  other. SQLite is a new build dependency.
- `catalist` with no command, into a pipe, prints its usage and exits
  with 2.
- `catalist import` takes MyAnimeList XML files, Catalist's JSON or the
  CSV files of 0.3.0 (`-` for standard input), reads them all before adding
  anything, reports per file how many titles were added, and is one step
  for undo.
- New titles are numbered from 1 in each list; an imported title merges
  with a tracked one only when they share an AniList, MyAnimeList or Kitsu
  id.
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
- The two-way sync of 0.3 with MyAnimeList and AniList, with `sync.csv`,
  the backups and the activity history (`history.csv`); tracking is now
  one way (see Added), and old logins must be made again.
- The Settings tab, `settings.json`, `catalist config`, themes, the command
  palette and the welcome message: the defaults are fixed.
- The CLI commands `count` and `random`, and `--tab`.
- `catalist.kdl` and its parser; an existing file is ignored, as are the
  old `settings.json`, `history.csv`, `sync.csv`, `catalist.log`,
  `mal_auth.json`, `anilist_auth.json` and `backups/`, which can be deleted.
- The Kitsu ids of 0.3's files (Kitsu ids are kept again, and filled in
  by `refresh`, `add` and `import`), and stored `queue_pos` values.
- Reading the 7-column and 16-column CSV layouts of development builds
  before 0.2.1; `import` names such a file and reads nothing of it (open
  and save it once with Catalist 0.3.0 to convert it). Files from 0.2.1 to
  0.3.0 are read, `0000-00-00` dates included.

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
