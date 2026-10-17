# catalist

Your anime and manga watchlist, in the terminal. A fast local TUI and a
pipe-friendly CLI in one C++ binary, built with [FTXUI](https://github.com/ArthurSonzogni/FTXUI).
Track what you watch and read, rate it, and bring your whole list over from
MyAnimeList.

Everything stays on your machine. No account, no server, no network needed.

[Install](#install) · [Quick start](#quick-start) · [Importing from MAL](#importing-from-mal) ·
[The terminal app](#the-terminal-app) · [CLI mode](#cli-mode) ·
[Configuration](#configuration) · [Project layout](#project-layout) · [Roadmap](#roadmap)

> **Status:** early development, built as the final project for CC 202
> (C++ Programming 1). Sections marked *(planned)* describe where the project
> is heading, not what ships today.

## What you get

- **`catalist`, the app.** A multi-pane TUI for your lists, with the look and
  feel of ncmpcpp and btop: a list view, a details pane, and a status bar,
  all drawn with FTXUI.
- **Vim motions and mouse support.** `j`/`k`, `gg`/`G`, `/` search and `:`
  commands, or just click and scroll. Use whichever you like, or both.
- **MAL import.** Bring your existing anime and manga list over from a
  MyAnimeList export.
- **A CLI mode** *(planned)*. Nushell-style commands that pipe in and out:
  filter by rating, list chronologically, change a rating or status in one
  line.
- **Local first.** Your list is a file you own. Nothing leaves your machine.

## Install

`catalist` builds into a single binary. Pick one.

**Nix (recommended).** With the [Nix package manager](https://nixos.org/download/)
and flakes enabled, nothing else needs to be installed:

```bash
nix run github:<your-username>/catalist
```

Or from a checkout:

```bash
nix run .
```

**Build from source.** You need a C++17 compiler, CMake 3.20 or newer, and
git. FTXUI is fetched automatically during the configure step.

```bash
git clone https://github.com/<your-username>/catalist
cd catalist
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/catalist --version
```

**Nix development shell.** To get the compiler, CMake and FTXUI in one
reproducible environment:

```bash
nix develop
cmake -B build && cmake --build build
```

`catalist --version` confirms the install.

## Quick start

```bash
catalist                      # open the TUI
catalist import mal ~/animelist.xml
```

To leave the TUI, press `q` (or `:q` then `Enter`). Changes are saved as you
make them.

Inside the TUI:

| Keys | What |
| --- | --- |
| `j` `k` | Move down and up the list |
| `gg` `G` | Jump to the top and bottom |
| `h` `l` | Move between panes (or switch tabs) |
| `/` | Search your list |
| `Enter` | Open the selected entry |
| `:` | Command line |
| `?` | Keys for the current view |
| `q` | Quit |

## Importing from MAL

On MyAnimeList, export your list from your profile's export page. It gives
you an XML file for anime and another for manga.

```bash
catalist import mal ~/animelist.xml
catalist import mal ~/mangalist.xml
```

Titles, status, score, progress, and dates come across. Importing the same
file twice does not duplicate entries. Importing is done entirely from the
file you give it, so your MAL credentials are never needed.

Support for other sources such as AniList is *(planned)*.

## The terminal app

```bash
catalist                      # the default list
```

The layout borrows from ncmpcpp and btop. Each pane is a framed box with
its title on the top edge, and the active pane is highlighted.

| Pane | Shows |
| --- | --- |
| Library | Your entries, filtered and sorted |
| Details | Status, rating, progress, and dates for the selected entry |
| Status bar | The current mode, filter, and any messages |

### Moving around

Everything works from the keyboard with vim-style motions, and everything also
works with the mouse: click to focus and select, double click to open, and
scroll with the wheel. Mouse support can be turned off in the
[configuration](#configuration).

### Editing an entry

Select an entry and change it in place:

| Keys | What |
| --- | --- |
| `r` | Set the rating (0 to 10) |
| `s` | Set the status (watching, completed, on hold, dropped, planned) |
| `+` `-` | Increase or decrease progress by one |
| `d` | Remove the entry (asks first) |

## CLI mode

*(planned)* The same binary doubles as a scriptable command line. Commands
read and write plain text or JSON, so they chain together with pipes the way
they would in nushell:

```bash
catalist ls                                   # everything, as a table
catalist ls --sort started                    # chronological
catalist ls --json | jq '.[] | select(.rating <= 7)'
catalist ls --rating "<=7"                    # 7/10 and below
catalist ls --rating 5                        # exactly 5/10
catalist set "Frieren" --rating 9 --status completed
```

Exit codes are `0` for success, `1` for an operation failure, and `2` for a
usage error. `--json` prints machine-readable output on stdout.

## Configuration

*(planned)* Settings live in a single config file under
`~/.config/catalist/config.toml`.

| Key | What |
| --- | --- |
| `mouse` | Turn mouse support on or off |
| `vim_motions` | Turn vim-style keys on or off |
| `theme` | Color scheme for the TUI |
| `data_file` | Where your list is stored |

Your list lives in a local data file under the XDG data directory
(`~/.local/share/catalist/` by default). It never leaves your machine.

## Project layout

```
src/main.cpp          entry point, argument parsing
src/ui/               FTXUI components: panes, list, details, status bar
src/model/            entries, statuses, ratings
src/storage/          reading and writing the local data file
src/import/           MAL import
src/cli/              CLI mode commands and output formatting
flake.nix             Nix package, dev shell, and `nix run` entry point
CMakeLists.txt        build, with FTXUI fetched at configure time
```

## Roadmap

- [x] Pick the stack: C++ with FTXUI, Nix for easy running
- [ ] List view, details pane, and status bar
- [ ] Add, edit, and remove entries
- [ ] Vim motions and mouse support
- [ ] MAL import
- [ ] CLI mode with filtering, sorting, and quick edits
- [ ] Piping in and out with `--json`
- [ ] Syncing with other sources such as AniList

## Contributing

This is a class project, but ideas and bug reports are welcome. Open an issue
with what you expected and what happened.

## License

MIT
