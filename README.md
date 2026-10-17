# Catalist

Your anime and manga watchlist, in the terminal. A fast local TUI and a
pipe-friendly CLI in one C++ binary, built with [FTXUI](https://github.com/ArthurSonzogni/FTXUI).
Track what you watch and read, rate it, and bring your whole list over from
MyAnimeList.

By default everything stays on your machine. No account, no server, no
network needed. If you want your list on more than one device, a
[self-hosted server](#self-hosted-server) is planned.

<img width="1920" height="1080" alt="image" src="https://github.com/user-attachments/assets/c058cf86-98c6-422d-b69c-281c634d7a91" />

[Install](#install) · [Quick start](#quick-start) · [Importing from MAL](#importing-from-mal) ·
[The terminal app](#the-terminal-app) · [CLI mode](#cli-mode) ·
[Self-hosted server](#self-hosted-server) ·
[Configuration](#configuration) · [Project layout](#project-layout) · [Roadmap](#roadmap)

> **Status:** early development, built as the final project for CC 202
> (C++ Programming 1). Sections marked *(planned)* describe where the project
> is heading, not what ships today.

## What you get

- **`catalist`, the app.** A unified floating-card TUI for your watchlists.
  Switch between media tabs with `1`, `2`, and `3`: Anime (`󰴙`), Manga (`󱥬`),
  and Statistics Overview (`󰄫`), all drawn with FTXUI.
- **APA-style minimalist catalog.** A table layout with zero border T-junctions
  or horizontal rule clutter. Column headers and entry rows align down to the
  individual character.
- **Vim motions and deep mouse support.** `j`/`k`, `gg`/`G`, `/` search with live
  case-insensitive yellow highlighting, or just click and scroll. Scrub progress
  directly on the gauge, click star clusters to rate, and step episodes with `[-]`/`[+]`.
- **Zero-prompt record creation.** Instant entry creation with `n` or `a`
  without tedious multi-field modal prompts.
- **MAL import.** Ingest existing anime and manga lists directly from a
  MyAnimeList XML export.
- **A CLI mode** *(planned)*. Nushell-style commands that pipe in and out:
  show only your 9/10s, sort by rating or date, change a rating or status in
  one line, export to JSON or CSV.
- **Local first.** Your list is stored in plain CSV files you own under
  `~/.config/catalist/`. Nothing leaves your machine unless you set up a
  server yourself.
- **Self-hosted server** *(planned)*. Run `catalist serve` on your own
  machine or home server and use the same TUI and CLI against it from any
  device.

## Install

`catalist` builds into a single binary. Pick one.

**Nix (recommended).** With the [Nix package manager](https://nixos.org/download/)
and flakes enabled, nothing else needs to be installed:

```bash
nix run github:McKaigne/Catalist
```

Or from a checkout:

```bash
nix run .
```

**Build from source.** You need a C++17 compiler, CMake 3.20 or newer, and
git. FTXUI is fetched automatically during the configure step.

```bash
git clone https://github.com/McKaigne/Catalist
cd Catalist
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/catalist --version
```

**Nix development shell.** To get the compiler, CMake, and FTXUI in one
reproducible environment:

```bash
nix develop
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
```

`catalist --version` confirms the install.

## Quick start

```bash
catalist                      # open the interactive TUI
catalist import ~/animelist.xml
catalist import --clean ~/animelist.xml ~/mangalist.xml
```

To leave the TUI, press `q` (or `:q` then `ret`). Changes are saved to disk as you
make them.

Inside the TUI:

| Keys | Action |
| --- | --- |
| `1` `2` `3` | Jump directly to Anime (`󰴙`), Manga (`󱥬`), or Overview (`󰄫`) |
| `Tab` / `[` `]` | Cycle between tabs |
| `j` `k` / Wheel | Move up and down the catalog with smooth viewport tracking |
| `gg` `G` | Jump to the first and last entry |
| `c` `t` `s` `r` `p` | Sort by Date, Title, Status, Rating, or Progress (tap again to flip) |
| `n` `a` | Instantly create a new record without manual input forms |
| `d` | Delete selected entry (prompts `[y/N]` confirmation, undoable with `u`) |
| `ret` | Enter in-place cell edit mode (`h`/`l` column, `j`/`k` adjust) |
| `y` | Copy selected title to system clipboard (`wl-copy` / OSC 52) |
| `/` | Search catalog with live case-insensitive yellow match highlighting |
| `o` | Open selected record directly on MyAnimeList in default browser |
| `u` / `Ctrl+Z` | Undo last edit or deletion |
| `Ctrl+R` / `Ctrl+Y` | Redo last undone action |
| `Esc` / `Backspace` | Step back to Categories sidebar |
| `:` | Open command line (`:w`, `:q`, `:undo`, `:backup`, `:sync`) |
| `q` | Save all lists and quit |

## Importing from MAL

On MyAnimeList, export your list from your profile's export page. It gives
you an XML file for anime and another for manga.

```bash
catalist import ~/animelist.xml
catalist import ~/mangalist.xml
```

To wipe the existing category and replace it cleanly:

```bash
catalist import --clean ~/animelist.xml
```

Titles, status, score, progress, and dates come across. Importing the same
file twice does not duplicate entries: each one is matched by MyAnimeList's own
database id and type, and kept as `mal_id` on the entry. Importing is done entirely from the
file you give it, so your MAL credentials are never needed. Backup snapshots are
automatically archived to `~/.config/catalist/backups/`.

Support for other sources such as AniList is *(planned)*.

## The terminal app

```bash
catalist                      # launch interactive watchlist
```

The interface is organized into decoupled, floating cards styled in Catppuccin Mocha.
A top tab bar switches media profiles, a sidebar indexes categories and rating tiers,
an APA-style minimalist table displays records with character-level alignment and
zero border T-junctions, a detail card inspects entries, and a bottom status ribbon
displays layman mode descriptors (`BROWSE`, `SEARCH`, `EDIT`, `NOTES`, `COMMAND`).

### Tabs

Switch profiles with number keys, click the tabs directly, or cycle with `Tab` or `[` and `]`:

| Key | Tab | Description |
| --- | --- | --- |
| `1` | Anime (`󰴙`) | Anime catalog, categorized views, and episode progress |
| `2` | Manga (`󱥬`) | Manga catalog, reading progress, and volume tracking |
| `3` | Overview (`󰄫`) | Total time spent, score distributions, and status progress bars |

*(Note: Screen numbers `4` to `8` covering dedicated browse, editor, and remote sync screens remain planned for future releases).*

### Navigation & Keybindings

#### Browse Mode (`[ BROWSE ]`)

The default catalog navigation mode:

| Keys | Action |
| --- | --- |
| `j` `k` / Mouse Wheel | Move up and down the list |
| `gg` `G` | Jump to first and last entry |
| `c` `t` `s` `r` `p` | Sort by Date, Title, Status, Rating, or Progress (tap again to flip order) |
| `n` `a` | Create a new record instantly |
| `d` | Delete selected entry (prompts `[y/N]` confirmation) |
| `ret` | Enter in-place cell edit mode |
| `y` | Copy selected title to system clipboard (`wl-copy` / OSC 52) |
| `/` | Focus search bar |
| `o` | Open record on MyAnimeList in default browser |
| `u` / `Ctrl+Z` | Undo last action |
| `Ctrl+R` / `Ctrl+Y` | Redo last action |
| `Esc` / `Backspace` | Move focus to Categories sidebar |
| `:` | Open command line |
| `q` | Save and quit |

#### Search Mode (`[ SEARCH ]`)

Activated by pressing `/` or clicking the search card. Hotkeys are suspended so all
alphanumeric input goes directly into the search buffer:

| Keys | Action |
| --- | --- |
| *Typed characters* | Live filter with case-insensitive yellow match highlighting on screen |
| `ret` / `ArrowDown` | Apply filter and return focus to the catalog rows |
| `Esc` | Clear search query and return to Browse mode |

#### In-Place Cell Edit Mode (`[ EDIT <FIELD> ]`)

Press `ret` on any catalog entry to adjust values without modal windows:

| Keys | Action |
| --- | --- |
| `h` `l` | Switch active column between `Progress`, `Rating`, and `Status` |
| `j` `k` / `+` `-` | Decrement or increment field value in place |
| `ret` | Open Title and Notes editor in the detail card |
| `Esc` / `Backspace` | Commit edits and return to Browse mode |

#### Detail Card Editor (`[ NOTES ]`)

| Keys | Action |
| --- | --- |
| `Tab` / `Down` | Move between Title input and Notes input |
| `ret` (on Notes) / `Esc` | Commit title and notes to disk and return to Browse mode |

### Mouse Interactivity

Everything can be controlled with the mouse:
- **Tabs:** Click `Anime`, `Manga`, or `Overview` to switch views.
- **Search & Creation:** Click the search card to type; click `[+ New]` to create an entry.
- **Sorting:** Click table column headers (`Date`, `Title`, `Status`, `Rating`, `Progress`) to toggle sort order; active sort columns display natural directional indicators (`▲` / `▼`) with clean underline styling.
- **Selection & Hover:** Moving the cursor over the table auto-highlights the entry under the pointer. Titles longer than the column width marquee-scroll smoothly.
- **Scrolling:** The mouse wheel scrolls up and down the list smoothly without snapping to stationary pointer positions.
- **Detail Card Controls:**
  - Click `[-]` or `[+]` to step episodes/chapters.
  - Click anywhere along the progress gauge to scrub progress to that exact percentage.
  - Click directly across the 5-star cluster to set scores between `0/10` and `10/10`.
  - Click the status badge to cycle watch/read status.
  - Click `[Delete]` to trigger confirmation.
  - Click Title or Notes to open the inline text editor.
  - Click metadata fields (Title, ID, Dates) to copy their values to the system clipboard.
- **Sidebar:** Click any category or star rating tier to apply the filter and focus the catalog.

## CLI mode

*(planned)* The same binary doubles as a scriptable command line. Commands
read and write plain text or structured output, so they chain together with
pipes the way they would in nushell. Run `catalist <command> --help` for
scoped usage.

### Commands

| Area | Commands |
| --- | --- |
| Browse | `ls` `show` `count` `random` |
| Edit | `add` `set` `rm` |
| Data | `import` `export` |
| Insight | `stats` |
| Setup | `config` `completion` `doctor` `--version` |
| Server | `serve` `connect` `disconnect` `use` *(see [Self-hosted server](#self-hosted-server))* |

### Listing and filtering

```bash
catalist ls                                   # everything, as a table
catalist ls --type anime                      # anime only (or manga)
catalist ls --status watching                 # watching, reading, completed,
                                              # on-hold, dropped, planned
catalist ls --rating 9                        # exactly 9/10
catalist ls --rating ">=9"                    # 9/10 and up
catalist ls --rating "<=7"                    # 7/10 and below
catalist ls --rating 6..8                     # a range, inclusive
catalist ls --unrated                         # entries with no rating yet
catalist ls --year 2024                       # by year started
catalist ls --search "frieren"                # title contains
catalist ls --tag favorite                    # by tag
catalist ls --limit 10                        # first ten results
```

Filters combine. For example, the best finished anime of one year:

```bash
catalist ls --type anime --status completed --year 2023 --rating ">=9"
```

In a terminal, a list is a table with a header, a rule under it, and a count
at the bottom:

```text
$ catalist ls --sort rating --reverse --limit 6

  ID  TITLE         TYPE   STATUS     RATING  PROGRESS
 ───  ────────────  ─────  ─────────  ──────  ────────
   4  Frieren       anime  completed   10/10     28/28
  17  Vinland Saga  anime  completed    9/10     48/48
 112  Berserk       manga  reading      9/10     364/?
  38  Mushishi      anime  completed    9/10     46/46
   9  Odd Taxi      anime  completed    8/10     13/13
 131  Chainsaw Man  manga  reading      8/10     190/?

 6 of 142 entries
```

### IDs

Every entry has a plain whole-number ID, like `4` or `131`. It is assigned when
the entry is added, never reused, and never changes, so you can type
`catalist set 131 --rating 9` or keep it in a script.

| Field | What |
| --- | --- |
| `id` | catalist's own number, local to your list |
| `mal_id` | The MyAnimeList database id, filled in by `import mal` |
| `anilist_id` | The AniList id, for future sync *(planned)* |

```bash
catalist show 131                             # by catalist id
catalist show --mal-id 52991 --type anime     # by MyAnimeList id
catalist ls --mal-id 52991 --type anime --ids # find your id for it
```

### Ordering

```bash
catalist ls --sort rating                     # lowest to highest
catalist ls --sort rating --reverse           # highest first
catalist ls --sort started                    # chronological by start date
catalist ls --sort finished --reverse         # most recently finished first
catalist ls --sort title                      # alphabetical
catalist ls --sort progress                   # by how far along you are
catalist ls --sort updated --reverse          # what you touched last
```

Sort keys are `title`, `rating`, `started`, `finished`, `progress`,
`updated`, and `year`. Add `--reverse` to flip the order.

### Looking at one entry

```bash
catalist show "Frieren"                       # all fields for one entry
catalist count --status dropped               # how many match, as a number
catalist random --status planned              # pick something to watch next
catalist stats                                # totals, averages, per-status counts
```

### Quick edits

```bash
catalist add "Frieren" --type anime --status planned --episodes 28
catalist set "Frieren" --rating 9 --status completed
catalist set "Frieren" --progress 12          # set progress directly
catalist set "Frieren" --progress +1          # or step it
catalist set "Frieren" --started 2023-10-01 --finished 2024-03-22
catalist set "Frieren" --tag favorite --note "rewatch with friends"
catalist rm "Frieren" --yes                   # permanent; asks without --yes
```

### Output formats and pipes

```bash
catalist ls --json                            # structured output on stdout
catalist ls --csv > list.csv                  # spreadsheet-friendly
catalist ls --ids                             # one id per line, for scripting
catalist ls --rating ">=9" --ids | catalist set - --tag favorite
printf 'Frieren\n' | catalist show -          # read titles or ids from stdin
catalist ls --plain                           # one title per line, no table
```

### Exporting and importing

```bash
catalist export json > backup.json
catalist export csv > backup.csv
catalist import ~/animelist.xml
catalist import json backup.json
```

### Shell completion

```bash
catalist completion bash > ~/.catalist-completion.bash
source ~/.catalist-completion.bash
```

## Self-hosted server

*(planned)* Your list stays local by default. If you want the same list on
your laptop, desktop, and phone terminal, you will be able to run your own
server. There is no hosted service and no account: you run it, you own the
data.

```bash
catalist serve --data ~/catalist-data         # start a server on this machine
catalist connect https://list.example.com --name home
catalist --server home ls --rating ">=9"      # any command against the server
catalist --server home                        # the TUI against the server
catalist use home                             # make it the default
catalist use local                            # and back
```

## Configuration

*(planned)* Settings live in a single config file under
`~/.config/catalist/config.toml`.

| Key | What |
| --- | --- |
| `mouse` | Turn mouse support on or off |
| `theme` | Color scheme for the TUI |
| `default_media` | Default startup media (`anime` or `manga`) |
| `default_sort` | Default sort key (`score`, `date`, `title`, `progress`) |
| `reverse_sort` | Default sort direction (`true` or `false`) |

Your watchlist lives in local CSV files under the XDG config directory
(`~/.config/catalist/` by default).

## Project layout

```
src/main.cpp          entry point, CLI argument parsing, import dispatcher
src/tui.cpp           interactive dashboard, floating cards, mouse engine, key handling
src/database.cpp      CRUD operations, filtering, in-memory models, CSV persistence
src/entry.cpp         data definitions, formatting, Nerd Font icons, star meter
src/mal_parser.cpp    MyAnimeList XML import engine with CDATA extraction
src/sync.cpp          upstream metadata sync via Jikan API
src/config.cpp        configuration loading and serialization
include/catalist/     header prototypes and Catppuccin Mocha theme definitions
flake.nix             Nix flake package and development environment
CMakeLists.txt        CMake build definition with automated FTXUI fetching
```

## Roadmap

- [x] Pick the stack: C++ with FTXUI, Nix for easy running
- [x] Tab bar and numbered profiles (`1` Anime, `2` Manga, `3` Overview) with `Tab` cycling
- [x] Decoupled card layout with minimal APA table, details pane, and status bar
- [x] Instant add (`n`/`a`), in-place cell edit (`ret`), and delete (`d`)
- [x] Vim motions, clamped navigation, and deep mouse support
- [x] Live search with case-insensitive substring highlighting
- [x] System clipboard integration via `wl-copy` and OSC 52
- [x] MyAnimeList XML import engine with automated backup snapshots
- [ ] Numbered screen tabs `4` to `8` (dedicated Browse, Editor, Sources)
- [ ] CLI `ls` with filters (type, status, rating, year, tag) and sorting
- [ ] CLI quick edits: `add`, `set`, `rm`, progress stepping
- [ ] Piping in and out: `--json`, `--csv`, `--ids`, stdin with `-`
- [ ] `export` and `import` for JSON and CSV
- [ ] `stats`, `count`, and `random`
- [ ] Shell completion
- [ ] Local and remote backends behind one interface
- [ ] `catalist serve` with token authentication
- [ ] `catalist connect` and `--server` for the TUI and CLI
- [ ] Nix module, systemd unit, and container image for hosting
- [ ] Syncing with other sources such as AniList

## Contributing

This is a class project, but ideas and bug reports are welcome. [Open an issue](https://github.com/McKaigne/Catalist/issues)
with what you expected and what happened.

## License

MIT
