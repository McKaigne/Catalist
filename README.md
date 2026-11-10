# Catalist

A fast, lightweight, and local-first anime and manga watchlist TUI built with **C++17** and **FTXUI**.

Catalist keeps your watchlist saved on your computer in plaintext RFC 4180 CSV files. It links each entry to its MyAnimeList and AniList pages, supports two-way sync with your MyAnimeList and AniList lists, and an integrated headless CLI for scripting.

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
| `3` | Stats | Status breakdown, totals, score and start-year charts, recent activity (`j`/`k`/`m` toggle anime/manga) |
| `4` | Settings | Themes (live preview), preferences, MyAnimeList and AniList accounts with two-way sync, import/export, backups |

Every tab uses the same cards: a sidebar on the left, the main cards in the
middle and a details card on the right.

The startup tab is set in Settings → Preferences, or with
`catalist config set startup_tab stats`.

## Core Keybindings

### Everywhere
| Key | Action |
| :--- | :--- |
| `1`–`4` | Switch to the numbered tab |
| `Tab` / `S-Tab` | Next / previous tab |
| `j` / `k`, `g` / `G` | Move down / up, jump to top / bottom (every tab) |
| `h` / `l` | Move between panes, columns or panels |
| `:` | Command palette (`:theme`, `:sync`, `:backup`, `:import`, `:about`, `:tab <id>`, `:w`, `:q`) |
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
| `o` / `y` | Open in browser / copy the title |
| `c` `t` `s` `r` `p` | Sort by date, title, status, rating or progress (press again to reverse) |

### Stats and Settings
| Key | Action |
| :--- | :--- |
| `j` / `k` / `m` | Stats: toggle anime / manga |
| `h` / `l` | Settings: move between the Themes, Preferences, Accounts & Sync and Data cards |
| `Enter` / `Space` | Settings: apply the theme, cycle the preference or run the action under the cursor |

---

## MyAnimeList & AniList

Log in under **Settings → Accounts & Sync** (or `catalist login mal|anilist`),
then choose **Sync now** (or `:sync`, or `catalist sync`). Sync is two-way:

- Catalist remembers, for every entry and list, the values both sides last
  agreed on, so it can tell which side changed;
- entries on your online lists that Catalist lacks are added, and entries
  Catalist has that a list lacks are sent to it;
- a change made on one side reaches the other, field by field, so an edit
  here and a different edit online are both kept;
- if the same field changed on both sides, Catalist keeps its own value and
  the sync report names the entry and the field;
- the first time an entry meets a list, whichever side changed more recently wins;
- deleting a synced entry in Catalist deletes it online, and an entry deleted
  online is removed from Catalist (unless you changed it here since; then it
  is sent back and reported);
- if a list comes back empty while Catalist has synced entries for it,
  nothing is changed for that list;
- a backup snapshot is written before every sync.

MyAnimeList ids and AniList ids are matched through AniList, so entries move
between the two lists as well.

**AniList login** needs an API client of your own (free): create one at
<https://anilist.co/settings/developer> with the redirect URL
`https://anilist.co/api/v2/oauth/pin`, then either export
`CATALIST_ANILIST_CLIENT_ID=<id>` or build with
`-DCATALIST_ANILIST_CLIENT_ID=<id>`. Logging in opens AniList in your browser;
paste the token it shows back into Catalist. Tokens last a year.

Without logging in you can still **import** a public list
(`catalist import anilist --user NAME`, `catalist import mal --user NAME`) and
**export** MyAnimeList XML (`catalist export xml --type anime`, or Settings →
Data), which both MyAnimeList's and AniList's import pages accept.

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
catalist export xml --type manga > manga.xml   # MAL format, for MAL or AniList import
catalist import --clean myanimelist.xml  # Ingest MAL export with safe merging
catalist import anilist --user someone   # Pull a public AniList list

# Accounts
catalist login mal                       # or: anilist
catalist sync                            # two-way, every logged-in list
catalist logout anilist
```

---

## Architecture & Data Safety

- **Local-First & Offline**: Your library is stored in `~/.config/catalist/anime.csv` and `manga.csv`. No background daemon or database setup required.
- **Atomic File Operations**: All disk writes write to a `.tmp` file before an atomic `rename`, preventing database corruption during power cuts or unexpected exits. A save that fails (a full disk, a read-only folder) is reported: the CLI exits with code 1 and the TUI shows a notice.
- **One Writer at a Time**: Each command loads the whole catalogue and saves it whole, so if the TUI and a CLI command that changes entries run at the same moment, one of the two changes can be lost.
- **CSV Columns**: `anime.csv` and `manga.csv` have the same 16 columns as `catalist export csv`, so they open directly in a spreadsheet. Files from Catalist 0.3 and earlier load and are rewritten in this layout on the next save; files from development builds before 0.2.1 must be opened and saved once with Catalist 0.3.0 first.
- **Tier 1 / Tier 2 Merge Hierarchy**: Re-importing MyAnimeList XML archives never overwrites local progress, scores, or notes. Upstream metadata updates refresh total episodes and MyAnimeList and AniList ids without clobbering user edits.
- **Preferences**: Theme, mouse support, default media and sort, and the startup tab are changed in Settings (or with `catalist config`) and saved to `~/.config/catalist/settings.json`. Keybindings are fixed.
- **Accounts**: Logins are kept in `mal_auth.json` and `anilist_auth.json` (readable only by you). What each entry looked like when it was last synced is kept in `sync.csv`.
- **Backups**: `Settings → Data` (or `:backup`) writes timestamped copies to `~/.config/catalist/backups/`; restoring one first backs up the current data.

See [CHANGELOG.md](CHANGELOG.md) for release notes and [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for design decisions.

---

## License

MIT
