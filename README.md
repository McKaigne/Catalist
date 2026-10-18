# Catalist

[![CI](https://github.com/McKaigne/Catalist/actions/workflows/ci.yml/badge.svg)](https://github.com/McKaigne/Catalist/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

<img width="1920" height="1080" alt="image" src="https://github.com/user-attachments/assets/3815eb46-3ea2-4688-a36e-7f1b062ca178" />

A fast, local terminal watchlist and manager for Anime and Manga built in C++17 with [FTXUI](https://github.com/ArthurSonzogni/FTXUI). Everything runs offline from local CSV files you own.

> **Font Recommendation:** A [Nerd Font](https://www.nerdfonts.com/) (e.g., JetBrainsMono Nerd Font) is recommended to render media and status glyphs (`󰴙`, `󱥬`, `󰅙`, ``).

---

## Quick Start

### Build & Run

```bash
# Via Nix (reproducible)
nix run github:McKaigne/Catalist

# From source (CMake 3.20+, C++17 compiler)
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
./build/catalist
```

### Import from MyAnimeList

Export your list from MyAnimeList (profile export page) as XML, then ingest it:

```bash
./build/catalist import animeList.xml
./build/catalist import mangaList.xml

# Clean-slate import (replaces existing records in that category)
./build/catalist import --clean animeList.xml
```

Records are deduplicated on `malId`. Automated backups are archived to `~/.config/catalist/backups/`.

---

## The Terminal App

```bash
catalist
```

The interface uses a decoupled, floating-card layout in Catppuccin Mocha with an APA-style minimalist table (zero vertical lines or border T-junction artifacts).

### Navigation & Keybindings

| Key | Mode | Action |
| :--- | :--- | :--- |
| `1` `2` `3` | Global | Jump directly to **Anime** (`󰴙`), **Manga** (`󱥬`), or **Overview** (`󰄫`) |
| `Tab` / `[` `]` | Global | Cycle media tabs |
| `j` `k` / Wheel | Browse | Move up/down with automatic viewport frame tracking |
| `gg` `G` | Browse | Jump to top/bottom of catalog |
| `c` `t` `s` `r` `p` | Browse | Sort by Date, Title, Status, Rating, or Progress (tap again to reverse) |
| `n` `a` | Browse | **New Entry:** Instant zero-prompt creation defaulting to active media |
| `d` | Browse | **Delete:** Prompts `[y/N]` confirmation (undoable with `u`) |
| `ret` | Browse | **Edit Row:** Enters in-place cell edit mode |
| `y` | Browse | **Copy Title:** Copies selected title to system clipboard |
| `/` | Browse | **Search:** Live case-insensitive search with yellow substring highlighting |
| `o` | Browse | Open entry on MyAnimeList in default browser |
| `u` / `Ctrl+Z` | Browse | Undo last mutation or deletion |
| `Ctrl+R` / `Ctrl+Y` | Browse | Redo last undone action |
| `Esc` / `Backspace` | Browse | Shift focus back to Categories sidebar |
| `:` | Browse | Command mode (`:w`, `:q`, `:undo`, `:backup`, `:sync`) |
| `q` | Browse | Save watchlists to disk and exit |

### Cell Editing (`ret`)

- `h` / `l`: Switch active cell between **Progress**, **Rating**, and **Status**.
- `j` / `k` (or `+` / `-`): Adjust field value in place without layout eviction.
- `ret`: Open Title and Notes editor in the Detail card.
- `Esc` / `Backspace`: Commit changes and return to Browse.

### Detail Card Editor

- `Tab` / `Down`: Switch from Title to Notes.
- `ret` (on Notes) / `Esc`: Save to disk and return to Browse.

---

## Mouse Support

- **Tabs & Search:** Click tabs to switch media profiles; click search to filter; click `[+ New]` to create a record.
- **Header Ribbon:** Click column headers (`Date`, `Title`, `Status`, `Rating`, `Progress`) to sort; active column displays directional arrows (`▲`/`▼`) with underline styling.
- **Row Hover & Wheel:** Hovering auto-highlights rows; titles exceeding width scroll smoothly; mouse wheel scrolls catalog without snapping to pointer.
- **Interactive Detail Card:**
  - Click `[-]` or `[+]` to step episodes/chapters.
  - Click directly along the progress gauge to scrub progress percentage.
  - Click across the 5-star cluster to set score (0 to 10).
  - Click Status badge to cycle status.
  - Click `[Delete]` to trigger deletion confirmation.
  - Click Title or Notes to edit inline.
  - Click metadata fields (or press `y`) to copy value to clipboard.
- **Clipboard Waterfall:** `y` and click-to-copy use Wayland (`wl-copy`), X11 (`xclip`), macOS (`pbcopy`), and ANSI OSC 52 sequences (remote SSH / Ghostty).

---

## Offline Authority & Upstream Sync

Catalist is **100% offline-first**. All records live locally in `~/.config/catalist/{anime,manga}.csv`. No accounts, telemetry, or network daemons exist.

The `:sync` command is an **optional, opt-in metadata reconciliation tool** that queries the public Jikan (MAL) API over HTTPS strictly on-demand to fetch total episode counts for ongoing series without credentials.

---

<details>
<summary><b>CLI Mode & Pipeline Usage (ls, count, show, add, set, rm, random)</b></summary>

When arguments are passed, `catalist` operates headlessly for shell scripting and Nushell pipelines:

```bash
# Query & Discovery
catalist ls --type anime --status watching --sort rating
catalist ls --rating ">=9" --plain | fzf
catalist count --status completed
catalist show "Frieren"
catalist random --status planned --plain | cowsay

# Quick Mutations
catalist add "Dungeon Meshi" -t anime -e 24 -p 10 -r 9
catalist set "Dungeon Meshi" -p a 2           # progress add 2
catalist set "Dungeon Meshi" -p sub 1         # progress sub 1
catalist set "Dungeon Meshi" -r 10 -s completed
catalist rm "Dungeon Meshi" -y                # remove with -y bypass

# Batch Pipeline Mutations (accepts stdin via -)
catalist ls --rating ">=9" --ids | catalist set - -n "Peak fiction"
catalist ls --status dropped --ids | catalist rm - -y
```

### CLI Flag Reference

| Flag | Shorthand | Description |
| :--- | :--- | :--- |
| `-t, --type <anime|manga>` | `-t` | Media type filter or assignment |
| `-s, --status <status>` | `-s` | `watching`, `reading`, `completed`, `on-hold`, `dropped`, `planned` |
| `-r, --rating <spec>` | `-r` | Filter: `9`, `'>=9'`, `'<=7'`, `'6..8'`, `'unrated'`. Set: `0..10` |
| `-p, --progress <val>` | `-p` | Set absolute, or step: `add [N]`, `a [N]`, `sub [N]`, `s [N]`, `+N`, `-N` |
| `-e, --total <N>` | `-e` | Total units (episodes / chapters) |
| `-v, --volumes <N>` | `-v` | Total volumes read |
| `-y, --year <YYYY>` | `-y` | Start year filter |
| `-q, --search <query>` | `-q` | Title substring filter |
| `--sort <field>` | | Sort: `date`, `title`, `status`, `rating`, `progress` |
| `-R, --reverse` | `-R` | Invert sort order |
| `-l, --limit <N>` | `-l` | Limit output to first N entries |
| `--plain`, `--ids` | | Machine outputs: bare titles or bare IDs |
| `--csv`, `--json` | | Structured serialization formats |

</details>

---

<details>
<summary><b>Planned Architecture (Self-Hosted Server)</b></summary>

### Self-Hosted Server Mode

```bash
catalist serve --data ~/catalist-data
catalist connect https://list.example.com --name home
catalist --server home ls --rating ">=9"
```

</details>

---

## Project Layout

```
src/main.cpp          Entry point and argument dispatcher
src/tui.cpp           Interactive dashboard, floating cards, mouse engine, key handling
src/cli.cpp           CLI query and mutation engine, stepping parser, JSON serializer
src/database.cpp      CRUD operations, RFC 4180 multiline CSV engine, filtering
src/entry.cpp         Data models, formatting, Nerd Font icons, star meter
src/malParser.cpp     MAL XML import engine with CDATA extraction
src/sync.cpp          Opt-in Jikan API upstream metadata sync
src/config.cpp        Configuration loader and serializer
tests/testMain.cpp    Headless verification suite (CSV, XML, sorting, CLI mutations)
flake.nix             Nix development environment and flake derivation
CMakeLists.txt        Build configuration with offline fallback and test targets
```

---

## Roadmap

- [x] C++17 foundation with FTXUI and Nix packaging
- [x] Pinned offline CMake fallback resolution
- [x] Decoupled card layout with minimal APA table (zero border T-junctions)
- [x] Instant add (`n`/`a`), cell edit (`ret`), and delete (`d`)
- [x] Clamped Vim motions, hover marquee, and deep mouse interaction
- [x] Live search with case-insensitive yellow match highlighting
- [x] Multi-tier clipboard engine (`wl-copy`, `xclip`, `pbcopy`, OSC 52)
- [x] RFC 4180 multiline CSV persistence engine
- [x] MAL XML ingestion engine with CDATA extraction
- [x] Headless verification test suite and GitHub Actions CI
- [x] CLI `ls` with filters (type, status, rating, year, tag), sorting, and output formats
- [x] CLI mutations (`add`, `set` with `add`/`sub` stepping, `rm`, `random`)
- [x] Pipeline support (`--plain`, `--ids`, `--json`, `--csv`, stdin `-`)
- [ ] Dedicated screen tabs `4` to `8` in TUI
- [ ] CLI export/import for standalone JSON/CSV dumps
- [ ] CLI analytics summary (`catalist stats`)
- [ ] Shell completion (Bash, Zsh, Fish)
- [ ] Polymorphic backend abstraction
- [ ] Self-hosted server daemon (`catalist serve`) with token authentication

---

## License

MIT
