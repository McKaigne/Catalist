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

## Core Keybindings (TUI)

### Navigation & Views
| Key | Action |
| :--- | :--- |
| `j` / `k` | Move cursor down / up |
| `h` / `l` | Move focus between Sidebar, Catalog, and Details panes |
| `g` / `G` | Jump to top / bottom of catalog list |
| `Tab` / `S-Tab` | Cycle through top tabs (Catalist, Anime, Manga, Overview, Sync, Settings) |
| `1` / `2` / `3` | Direct switch to Anime / Manga / Overview tabs |

### Editing & Progress
| Key | Action |
| :--- | :--- |
| `+` / `-` | Increment / decrement watched episodes or read chapters |
| `S` | Cycle status (`Watching` -> `Completed` -> `On-Hold` -> `Dropped` -> `Plan to Watch`) |
| `R` | Bump user rating score (`0` through `10`) |
| `e` / `Enter` | Edit entry notes / inspect details |
| `d` | Delete selected entry (prompts confirmation) |
| `u` / `U` | Undo / redo database mutations |

### Discovery & Integrations
| Key | Action |
| :--- | :--- |
| `/` | Enter search filter mode (exits back into catalog on `Enter` / `Esc`) |
| `a` | Search online remote providers (AniList / MAL) and add entry |
| `o` | Open active entry in default browser via resolved provider link |
| `y` | Yank / copy active entry title to system clipboard |
| `:` | Open Command Palette |
| `q` | Save and quit |

### Multi-Column Sorting
Pressing a sort hotkey toggles sort order between ascending and descending:
- `c` — Sort by start / completion Date
- `t` — Sort by Title
- `s` — Sort by Watch Status
- `r` — Sort by Rating Score
- `p` — Sort by Progress Count

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
- **KDL Workspace Configuration**: Declaratively customize theme, mouse support, default sorting, and layout via `~/.config/catalist/catalist.kdl`.

---

## License

MIT
