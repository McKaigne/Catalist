# Catalist

A small, local-first anime and manga tracker for the terminal, written in
**C++17** with **FTXUI**.

Your lists live on your computer in two plain CSV files. Catalist goes
online only to look up a title to add, to refresh what is known about a
title (how many episodes or chapters it has and how many are out) and to
show its cover, all through [AniList](https://anilist.co)'s public API,
which needs no account. AniList also knows every title's MyAnimeList id, so
each entry carries both, and Catalist works the same for AniList and
MyAnimeList users. Everything works offline too. Lists move in and out as
MyAnimeList XML, which both sites read.

## Provider

Your **provider** is the site you use: AniList (the default) or
MyAnimeList. Its ids are the ones Catalist shows and accepts, and its name
sits in the pill at the bottom left. Only one is chosen at a time. It is
kept in a `settings` file next to the lists, and is where syncing with
your account will plug in later (not done yet: lists are local only).

---

## Quickstart

### Nix

```bash
nix run github:McKaigne/Catalist
# or, in a clone:
nix run
```

### CMake

You need a C++17 compiler, CMake 3.20 or later, Ninja or Make; FTXUI,
doctest and fmt are used from the system or downloaded by CMake. At run time,
`curl` reaches AniList and `img2sixel` (from libsixel) draws covers;
without them Catalist works offline and without covers.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/catalist
ctest --test-dir build --output-on-failure
```

---

## Tabs

| Key | Tab | What it shows |
| :--- | :--- | :--- |
| `1` | Anime | Left: the filters with their counts (status, rating 10 to 0) and a stats card (days spent, mean score, episodes, rewatches, a status bar). Middle: the sortable list. Right: the cover, then the details and tracking cards |
| `2` | Manga | The same for manga, with chapters, rereads and volumes |

Covers come from AniList: Catalist asks for the cover's address by the
title's AniList id (or its MyAnimeList id, which AniList also knows),
downloads the picture with curl and turns it into sixel graphics with
`img2sixel`. They need a terminal that
shows sixel (foot, WezTerm, Konsole, xterm with `-ti vt340`, Windows
Terminal, and others), are fetched once the selection rests, and are kept
in memory only. When there is no picture, the cover card says why; a cover
that could not be fetched is tried again after a few seconds.

## Staying current

Whenever you change a title's status, rating or progress, Catalist asks
AniList about it (once a run), filling in whichever id it lacked: a series that has ended gets its final
episode or chapter count instead of `?`. Once a day, titles you are watching
or reading (or have on hold) are checked in the background, one request
every two seconds. The **New** column shows episodes or chapters that are
out but not yet watched, and the list starts with the titles that have the
most.
AniList only counts the chapters of a manga once it has ended, so new
chapters of a running manga cannot be known.

The bottom bar shows your provider, the last message and the keys that work
right now.

## Keys

| Key | Action |
| :--- | :--- |
| `1` `2`, `Tab` / `Shift-Tab` | Switch tab |
| `j` / `k`, `g` / `G` | Move down / up, jump to top / bottom |
| `h` / `l` | Move between the sidebar, the list and the details card |
| `+` / `-` (or `P`) | One episode or chapter more / less |
| `S` / `R` | Next status / one star more |
| `e` / `Enter` | Details card: `j`/`k` pick a field (status, rating, progress, total, volumes for manga, rewatches), `h`/`l` change it by one, `e`/`Enter` types a number |
| `n` `u` `c` `t` `s` `r` `p` | Sort by new releases (the default), last updated, date, title, status, rating or progress (again to reverse) |
| `/` | Filter by title |
| `a` | Add a title: type it, `Enter` looks it up on MyAnimeList, pick a match. The last row adds it by hand, which also works offline |
| `d` | Delete (asks first) |
| `q` | Quit |

Every change is saved the moment you make it.

## Command line

Everything the tracker does to a list also works from the shell:

```bash
catalist ls                                 # everything, new releases first
catalist ls manga --status reading          # one list, one status
catalist ls anime frieren                   # titles with these words
catalist add anime dandadan                 # search AniList, pick, add
catalist add anime 171018 -s watching -p 3  # by id, with a status and progress
catalist next frieren                       # one more episode or chapter
catalist next frieren -n 3                  # three more
catalist set 154587 -s completed -r 9       # change status, rating, progress...
catalist set berserk -p +12                 # progress forward (or -3 back)
catalist show taxi                          # one title in full
catalist rm "odd taxi"                      # asks first (or --yes)
catalist search manga berserk               # look without adding
catalist provider mal                       # show MyAnimeList ids instead
```

- **Naming a title:** by its id on your provider, or by words from its
  title (any case, in any order). Put `anime` or `manga` first to look in
  one list only. When several titles match, you are asked which one; in a
  script, the matches are listed and the command fails, so it never guesses.
- **Options** come in short and long forms (`-p 3`, `--progress 3`,
  `--progress=3`). Every command has `--help`, `catalist help` lists them,
  and a mistyped command or option gets a suggestion.
- **Output:** a table at a terminal (in colour, unless `NO_COLOR` is set);
  tab-separated lines when piped, for `cut`, `awk` or `sort`. Messages and
  questions go to stderr. Exit codes: 0 done, 1 failed, 2 wrong command line.
- **Staying current:** before a change, a title not checked today is looked
  up, so `catalist next` on a finished series gets its final count. Once a
  day the ids titles lack are looked up in batches of fifty (all of them,
  the first time after an import from MyAnimeList).

## Importing and exporting

```bash
catalist import animelist.xml mangalist.xml   # MyAnimeList XML exports
catalist export anime > anime.xml             # MyAnimeList XML on stdout
catalist export manga > manga.xml
```

- **MyAnimeList:** export your lists from MyAnimeList's export page, unpack
  the `.xml.gz` files (`gunzip`), and import them. Upload what
  `catalist export` writes on MyAnimeList's import page.
- **AniList:** AniList's settings have an import that reads MyAnimeList XML,
  so `catalist export` works there too. AniList does not export MyAnimeList
  XML itself; convert an AniList list with a third-party tool such as
  [AniPy](https://github.com/Jacekun/AniPy), then import that file.

Importing never overwrites what you track: a title already in the catalogue
(the same AniList or MyAnimeList id) keeps your score, progress and dates,
and only its totals are refreshed. Importing the same file twice adds nothing. Titles
without a MyAnimeList id are left out of an export, with a note on stderr.

---

## Data

- `~/.config/catalist/anime.csv` and `manga.csv` (or `$XDG_CONFIG_HOME/catalist`,
  or the directory in `CATALIST_DATA_DIR`). RFC 4180 CSV with 17 columns:
  `id, mal_id, type, title, status, score, progress, total_units, volumes,
  total_volumes, rewatched, start_date, finish_date, updated_at, released,
  checked_at, anilist_id` (the last three from the web checks; `id` is
  local and never shown).
  They open in any spreadsheet.
- Every save writes a temporary file and renames it over the old one, so a
  crash never leaves half a file. A save that fails is reported (the bottom
  bar in the TUI, exit code 1 in the CLI).
- Files written by Catalist 0.2.1 to 0.3.0 load as they are, AniList ids
  included; their notes and other extra columns are dropped on the next
  save.
- `settings` holds one line, `provider = anilist` or
  `provider = myanimelist`, and is only written when you change it.

The provider is the only setting. The theme is Catppuccin Mocha and the
list starts with new releases.

See [CHANGELOG.md](CHANGELOG.md) for release notes and
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for design decisions.

## License

MIT
