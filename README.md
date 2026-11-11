# Catalist

A small, local-first anime and manga tracker for the terminal, written in
**C++17** with **FTXUI**.

Your lists live on your computer in two plain CSV files. Catalist asks
[Jikan](https://jikan.moe), the public read-only MyAnimeList API, only to
look up a title to add, to refresh what is known about a title (how many
episodes or chapters it has and how many are out) and to show its cover;
everything works offline too. Lists move in and out as MyAnimeList XML,
which both MyAnimeList and AniList read.

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
`curl` reaches MyAnimeList and `img2sixel` (from libsixel) draws covers;
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

Covers are drawn with sixel graphics, so they need a terminal that shows
sixel (foot, WezTerm, Konsole, xterm with `-ti vt340`, Windows Terminal,
and others). They are fetched once the selection rests and are kept in
memory only.

## Staying current

Whenever you change a title's status, rating or progress, Catalist asks
MyAnimeList about it (once a run): a series that has ended gets its final
episode or chapter count instead of `?`. Once a day, titles you are watching
or reading (or have on hold) are checked in the background, one request a
second. The **New** column shows episodes or chapters that are out but not
yet watched, and the list starts with the titles that have the most.
MyAnimeList only counts the chapters of a manga once it has ended, so new
chapters of a running manga cannot be known.

The bottom bar shows the current mode (`NOR`, `SCH` search, `EDT` typing a
number, `ADD`, `DEL`), the last message and the keys that work right now.

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
(the same MyAnimeList id) keeps your score, progress and dates, and only its
totals are refreshed. Importing the same file twice adds nothing. Titles without a MyAnimeList id are left out of an
export, with a note on stderr.

---

## Data

- `~/.config/catalist/anime.csv` and `manga.csv` (or `$XDG_CONFIG_HOME/catalist`,
  or the directory in `CATALIST_DATA_DIR`). RFC 4180 CSV with 16 columns:
  `id, mal_id, type, title, status, score, progress, total_units, volumes,
  total_volumes, rewatched, start_date, finish_date, updated_at, released,
  checked_at` (the last two from the web checks).
  They open in any spreadsheet.
- Every save writes a temporary file and renames it over the old one, so a
  crash never leaves half a file. A save that fails is reported (the bottom
  bar in the TUI, exit code 1 in the CLI).
- Files written by Catalist 0.2.1 to 0.3.0 load as they are; their AniList
  ids, notes and other extra columns are dropped on the next save.

There are no settings: the defaults are the settings. The theme is
Catppuccin Mocha and the list starts with new releases.

See [CHANGELOG.md](CHANGELOG.md) for release notes and
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for design decisions.

## License

MIT
