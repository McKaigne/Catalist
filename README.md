# Catalist

A small, local-first anime and manga tracker for the terminal, written in
**C++17** with **FTXUI**.

Your lists live on your computer in two plain CSV files. Catalist only goes
online when you look up a title to add, through
[Jikan](https://jikan.moe), the public read-only MyAnimeList API. Lists move
in and out as MyAnimeList XML, which both MyAnimeList and AniList read.

---

## Quickstart

### Nix

```bash
nix run github:McKaigne/Catalist
# or, in a clone:
nix run
```

### CMake

You need a C++17 compiler, CMake 3.20 or later, Ninja or Make, and `curl` at
run time (only for adding titles from MyAnimeList).

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
| `1` | Anime | Sidebar filters (status, rating 10 to 0), the sortable list and a details card |
| `2` | Manga | The same for manga |
| `3` | Stats | Totals, the status breakdown and the score chart (`j`/`k`/`m` switch anime and manga) |

The bottom bar shows the current mode (`NOR`, `SCH` search, `EDT` notes,
`ADD`, `DEL`), the last message and the keys that work right now.

## Keys

| Key | Action |
| :--- | :--- |
| `1` `2` `3`, `Tab` / `Shift-Tab` | Switch tab |
| `j` / `k`, `g` / `G` | Move down / up, jump to top / bottom |
| `h` / `l` | Move between the sidebar, the list and the details card |
| `+` / `-` (or `P`) | One episode or chapter more / less |
| `S` / `R` | Next status / one star more |
| `e` / `Enter` | Details card: `j`/`k` pick a field, `h`/`l` change it, `Enter` on Notes edits them |
| `c` `t` `s` `r` `p` | Sort by date, title, status, rating or progress (again to reverse) |
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
(the same MyAnimeList or AniList id) keeps your score, progress, dates and
notes, and only its totals and ids are refreshed. Importing the same file
twice adds nothing. Titles without a MyAnimeList id are left out of an
export, with a note on stderr.

---

## Data

- `~/.config/catalist/anime.csv` and `manga.csv` (or `$XDG_CONFIG_HOME/catalist`,
  or the directory in `CATALIST_DATA_DIR`). RFC 4180 CSV with 16 columns:
  `id, mal_id, anilist_id, type, title, status, score, progress, total_units,
  volumes, total_volumes, rewatched, start_date, finish_date, notes, updated_at`.
  They open in any spreadsheet.
- Every save writes a temporary file and renames it over the old one, so a
  crash never leaves half a file. A save that fails is reported (the bottom
  bar in the TUI, exit code 1 in the CLI).
- Files written by Catalist 0.2.1 to 0.3.0 load as they are; their extra
  columns are dropped on the next save.

There are no settings: the defaults are the settings. The theme is
Catppuccin Mocha and the list starts sorted by rating.

See [CHANGELOG.md](CHANGELOG.md) for release notes and
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for design decisions.

## License

MIT
