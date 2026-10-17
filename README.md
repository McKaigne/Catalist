# catalist

Your anime and manga watchlist, in the terminal. A fast local TUI and a
pipe-friendly CLI in one C++ binary, built with [FTXUI](https://github.com/ArthurSonzogni/FTXUI).
Track what you watch and read, rate it, and bring your whole list over from
MyAnimeList.

By default everything stays on your machine. No account, no server, no
network needed. If you want your list on more than one device, a
[self-hosted server](#self-hosted-server) is planned.

[Install](#install) · [Quick start](#quick-start) · [Importing from MAL](#importing-from-mal) ·
[The terminal app](#the-terminal-app) · [CLI mode](#cli-mode) ·
[Self-hosted server](#self-hosted-server) ·
[Configuration](#configuration) · [Project layout](#project-layout) · [Roadmap](#roadmap)

> **Status:** early development, built as the final project for CC 202
> (C++ Programming 1). Sections marked *(planned)* describe where the project
> is heading, not what ships today.

## What you get

- **`catalist`, the app.** A multi-screen TUI for your lists. Jump between
  screens with the number keys `1` to `8`: library, in progress, search,
  browse, stats, and more, all drawn with FTXUI.
- **Vim motions and mouse support.** `j`/`k`, `gg`/`G`, `/` search and `:`
  commands, or just click and scroll. Use whichever you like, or both.
- **MAL import.** Bring your existing anime and manga list over from a
  MyAnimeList export.
- **A CLI mode** *(planned)*. Nushell-style commands that pipe in and out:
  show only your 9/10s, sort by rating or date, change a rating or status in
  one line, export to JSON or CSV.
- **Local first.** Your list is a file you own. Nothing leaves your machine
  unless you set up a server yourself.
- **Self-hosted server** *(planned)*. Run `catalist serve` on your own
  machine or home server and use the same TUI and CLI against it from any
  device.

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
| `1` to `8` | Jump straight to a screen |
| `Tab` `Shift+Tab` | Next and previous screen |
| `h` `l` | Move between panes within a screen |
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

The app is a set of full-window screens. A tab bar at the top names them,
the active one is highlighted, and a status bar at the bottom shows the
current mode, filter, and any messages.

### Screens

Press the number key to jump to a screen, or cycle with `Tab` and `Shift+Tab`.
Each screen remembers its cursor and filter while you are away.

| Key | Screen | Shows |
| --- | --- | --- |
| `1` | Help | Keys and commands for the current screen |
| `2` | Library | Every entry, with sorting and filtering, and a details pane for the selected one |
| `3` | In progress | Only what you are currently watching or reading |
| `4` | Search | Search by title, status, rating, or year, with results as you type |
| `5` | Browse | Entries grouped by status, type, or year |
| `6` | Stats | Counts, average rating, and progress at a glance *(planned)* |
| `7` | Editor | Edit every field of the selected entry in one place *(planned)* |
| `8` | Sources | Import from MAL, and later sync with other sites *(planned)* |

Not every screen has to exist at once. The set may change as the project
grows, but the number keys stay put.

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
at the bottom (colors not shown here):

```text
$ catalist ls --sort rating --reverse --limit 6

 ID   TITLE         TYPE   STATUS     RATING  PROGRESS
 ───  ────────────  ─────  ─────────  ──────  ────────
 a12  Frieren       anime  completed   10/10     28/28
 a07  Vinland Saga  anime  completed    9/10     48/48
 m31  Berserk       manga  reading      9/10     364/?
 a45  Mushishi      anime  completed    9/10     46/46
 a19  Odd Taxi      anime  completed    8/10     13/13
 m08  Chainsaw Man  manga  reading      8/10     190/?

 6 of 142 entries
```

Ratings and progress line up on the right, and an unknown total shows as
`?`. The `ID` column is the stable id you can pass to other commands.

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

Titles can be a partial match when it is unambiguous. If more than one entry
matches, the command stops and lists the candidates, or use the stable id
from `ls --ids`.

### Output formats and pipes

```bash
catalist ls --json                            # structured output on stdout
catalist ls --csv > list.csv                  # spreadsheet-friendly
catalist ls --ids                             # one id per line, for scripting
catalist ls --rating ">=9" --ids | catalist set - --tag favorite
printf 'Frieren\n' | catalist show -          # read titles or ids from stdin
catalist ls --plain                           # one title per line, no table
```

When the output is not a terminal, `catalist` drops colors and table borders
on its own, so it plays well with any program that reads text. `--plain`
gives you bare titles, one per line, on demand.

The same list as above, piped into `cowsay`. Piped, the rule under the header
and the count line are dropped, and the columns stay aligned. Use `cowsay -n`
so it does not re-wrap the table at 40 columns:

```text
$ catalist ls --sort rating --reverse --limit 6 | cowsay -n

 _______________________________________________________
/ ID   TITLE         TYPE   STATUS     RATING  PROGRESS \
| a12  Frieren       anime  completed   10/10     28/28 |
| a07  Vinland Saga  anime  completed    9/10     48/48 |
| m31  Berserk       manga  reading      9/10     364/? |
| a45  Mushishi      anime  completed    9/10     46/46 |
| a19  Odd Taxi      anime  completed    8/10     13/13 |
\ m08  Chainsaw Man  manga  reading      8/10     190/? /
 -------------------------------------------------------
        \   ^__^
         \  (oo)\_______
            (__)\       )\/\
                ||----w |
                ||     ||
```

More ideas:

```bash
catalist random --status planned --plain | cowsay   # a cow picks your next watch
catalist ls --status watching --plain | fzf         # pick one interactively
catalist ls --rating ">=9" --plain | wc -l          # how many 9s and up
catalist ls --sort finished --reverse --limit 5 | lolcat
```

Anything that reads from stdin works: `cowsay`, `fzf`, `sort`, `head`,
`grep`, `wc`, `less`, or a script of your own.

In nushell the JSON output becomes structured data you can keep filtering:

```nu
catalist ls --json | from json | where rating >= 9 | sort-by started
```

Elsewhere, `jq` works just as well:

```bash
catalist ls --json | jq '.[] | select(.rating <= 7) | .title'
```

### Exporting and importing

```bash
catalist export json > backup.json
catalist export csv > backup.csv
catalist import mal ~/animelist.xml
catalist import json backup.json
```

### Scripting conventions

Global flags work before or after the command, and `--` ends flag parsing.
`--no-input` makes a command fail instead of asking a question, which is what
you want in scripts. `NO_COLOR` turns off colors.

| Result | Exit code / output |
| --- | --- |
| Success | `0`; command data on stdout |
| Operation failure | `1` |
| Usage / argument error | `2` |
| `--json` failure | Structured error on stderr |

```json
{"error":{"code":"usage","message":"unknown flag --raiting. Did you mean \"rating\"?"}}
```

### Shell completion

```bash
catalist completion bash > ~/.catalist-completion.bash
source ~/.catalist-completion.bash
```

Zsh, Fish, and PowerShell are supported the same way with `catalist completion
zsh|fish|powershell`. Suggestions include commands, flags, sort keys,
statuses, and titles from your list.

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

What the server is meant to be:

- **The same binary.** `catalist serve` is a mode of the program you already
  have. There is nothing else to install.
- **Same everything, either backend.** The TUI, every CLI command, and the
  import and export tools work the same against a local file or a server.
- **Token authentication.** The server generates a token on first run.
  `catalist connect` asks for it once and stores it for you. Put the server
  behind HTTPS with a reverse proxy when it is exposed to the internet.
- **Simple to run.** It binds to loopback by default (`127.0.0.1`), and
  `--bind` and `--base-path` change that. A Nix module, a systemd unit, and a
  container image are planned so it can run on a home server.
- **Your data stays plain.** The server stores your list in the same local
  data format, so `export` and moving back to local-only always work.

Open questions for later: how to handle two devices editing the same entry
at once, and whether to offer a read-only web page for quick lookups.

The server is not required for anything else in this README. A local list
works fully offline, forever.

## Configuration

*(planned)* Settings live in a single config file under
`~/.config/catalist/config.toml`.

| Key | What |
| --- | --- |
| `mouse` | Turn mouse support on or off |
| `vim_motions` | Turn vim-style keys on or off |
| `theme` | Color scheme for the TUI |
| `data_file` | Where your list is stored |
| `default_sort` | Default sort key for `ls` and the Library screen |
| `default_workspace` | `local`, or the name of a saved server |

Your list lives in a local data file under the XDG data directory
(`~/.local/share/catalist/` by default). It never leaves your machine unless
you connect to a server you host yourself.

## Project layout

```
src/main.cpp          entry point, argument parsing
src/ui/               FTXUI components: tab bar, list, details, status bar
src/ui/screens/       one file per numbered screen (help, library, ...)
src/model/            entries, statuses, ratings
src/storage/          reading and writing the local data file
src/import/           MAL import
src/cli/              CLI mode commands, filters, sorting, output formatting
src/server/           self-hosted server mode (planned)
src/backend/          local and remote backends behind one interface (planned)
flake.nix             Nix package, dev shell, and `nix run` entry point
CMakeLists.txt        build, with FTXUI fetched at configure time
```

## Roadmap

- [x] Pick the stack: C++ with FTXUI, Nix for easy running
- [ ] Tab bar and numbered screens (`1` to `8`) with `Tab` cycling
- [ ] Library screen with details pane and status bar
- [ ] Add, edit, and remove entries
- [ ] Vim motions and mouse support
- [ ] MAL import
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

This is a class project, but ideas and bug reports are welcome. Open an issue
with what you expected and what happened.

## License

MIT
