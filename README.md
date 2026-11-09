# Catalist

A fast, lightweight, and local-first anime and manga watchlist TUI built with **C++17** and **FTXUI**.

Catalist keeps your watchlist saved on your computer in plaintext RFC 4180 CSV files. It supports modular metadata links across multiple providers (MyAnimeList, AniList, Kitsu), full KDL workspace configuration, and an integrated headless CLI for scripting.

---

## Quickstart

### 1. Running with Nix (Recommended)

Catalist is packaged as a pure Flake with runtime dependencies (`curl`, `wl-clipboard`, `xclip`, `xdg-utils`) pre-wrapped.

#### Run Directly (No Clone Required):
```bash
nix run github:<your-username>/catalist
```

#### Clone and Run:
```bash
git clone <repo-url> && cd catalist && nix run
```

#### Updating an Existing Clone:
```bash
git pull && nix run
```

---

### 2. Building with CMake

#### Prerequisites:
- C++17 compliant compiler (`gcc` >= 9 or `clang` >= 10)
- `cmake` (>= 3.20)
- `ninja` or `make`
- Runtime utilities: `curl`, `wl-clipboard` (Wayland) or `xclip` (X11), `xdg-utils`

#### Build and Run:
```bash
git clone <repo-url> && cd catalist
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/catalist
```

#### Running Tests:
```bash
ctest --test-dir build --output-on-failure
```

---

## Tabs (TUI)

Run `catalist` for the TUI, or `catalist --tab <id>` to open it on a specific tab.
The number keys follow the order shown in the top bar.

| Key | Tab | What it does |
| :--- | :--- | :--- |
| `1` | Anime | Your anime catalogue: sidebar filters, sortable table, details card |
| `2` | Manga | The same for manga |
| `3` | Queue | Watching and Plan to Watch entries in your own order (`J`/`K` to reorder) |
| `4` | Search | Online search across MyAnimeList, AniList and Kitsu; `Enter` adds |
| `5` | Library | Browse by start year, then status, then entries |
| `6` | History | Activity heatmap and a day-by-day log of updates |
| `7` | Stats | Totals, score histogram and entries per year (`m` toggles anime/manga) |
| `8` | Settings | Themes (live preview), preferences, backups, import/export, MAL login |
| `9` | Help | Every key, read live from your keybindings |

The tab list, its order and the startup tab are configured in `catalist.kdl`
(`top-bar` and `general { startup-tab "queue" }`).

## Core Keybindings

### Everywhere
| Key | Action |
| :--- | :--- |
| `1`–`9` | Switch to the numbered tab |
| `Tab` / `S-Tab` | Next / previous tab |
| `j` / `k`, `g` / `G` | Move down / up, jump to top / bottom (every tab) |
| `h` / `l` | Move between panes, columns or panels |
| `:` | Command palette (`:theme`, `:sync`, `:backup`, `:import`, `:about`, `:tab <id>`, `:w`, `:q`) |
| `?` | Open Help |
| `q` | Save and quit |

### Catalogue (Anime / Manga)
| Key | Action |
| :--- | :--- |
| `+` / `-` | Increment / decrement watched episodes or read chapters |
| `S` / `R` / `P` | Cycle status / bump rating / add progress (applies to the visual selection) |
| `Space` / `v` | Toggle visual (batch) selection |
| `e` / `Enter` | Open the details card; `e` on Notes edits them |
| `d` | Delete (asks to confirm; deletes the whole selection in visual mode) |
| `u` / `U` | Undo / redo |
| `/` | Filter by title |
| `a` | Add an entry (opens the Search tab) |
| `o` / `y` | Open in browser / copy the title |
| `c` `t` `s` `r` `p` | Sort by date, title, status, rating or progress (press again to reverse) |

### Queue, Search, Library, Stats
| Key | Action |
| :--- | :--- |
| `J` / `K` | Queue: move the entry down / up |
| `/`, `p` or `h`/`l` | Search: type a query, switch provider |
| `m` | Search, Library, Stats: toggle anime / manga |

---

## Command-Line Interface (CLI)

Catalist includes a headless CLI for scripting, shell pipes, and data analysis:

```bash
# Querying
catalist ls --type anime --status watching
catalist ls --rating ">=8" --sort rating --reverse
catalist ls --plain                     # Output bare titles (useful for dmenu/fuzzel)
catalist show "Frieren"                 # Inspect metadata
catalist stats --json                   # Output telemetry JSON

# Mutations
catalist add "Dungeon Meshi" -t anime -s watching -p 12 -e 24 -r 9
catalist set "Dungeon Meshi" -p +1      # Step episode progress
catalist set "Dungeon Meshi" -r 10      # Update score
catalist rm "Dungeon Meshi" -y          # Delete entry

# Backup & Interop
catalist export json > backup.json
catalist export csv > backup.csv
catalist import --clean myanimelist.xml  # Ingest MAL export with safe merging
```

---

## Architecture & Data Safety

- **Local-First & Offline**: Your library is stored in `~/.config/catalist/anime.csv` and `manga.csv`. No background daemon or database setup required.
- **Atomic File Operations**: All disk writes write to a `.tmp` file before an atomic `rename`, preventing database corruption during power cuts or unexpected exits.
- **Tier 1 / Tier 2 Merge Hierarchy**: Re-importing MyAnimeList XML archives never overwrites local progress, scores, or notes. Upstream metadata updates refresh total episodes and provider IDs without clobbering user edits.
- **KDL Workspace Configuration**: Declaratively customize theme, mouse support, default sorting, startup tab, tab order and keybindings via `~/.config/catalist/catalist.kdl`. Files from older versions load as-is.
- **Backups**: `Settings → Data` (or `:backup`) writes timestamped copies to `~/.config/catalist/backups/`; restoring one first backs up the current data.

See [CHANGELOG.md](CHANGELOG.md) for release notes and [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for design decisions.

---

## License

MIT
