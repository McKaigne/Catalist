# Catalist

A small, local-first anime and manga tracker for the terminal, written in
**C++17** with **FTXUI**.

Your lists live on your computer in one SQLite file. Catalist goes online
only to look up a title to add, to refresh what is known about a title
(how many episodes or chapters it has and how many are out) and to show its
cover, all through [AniList](https://anilist.co)'s public API, which needs
no account. AniList also knows every title's MyAnimeList id, so each entry
carries both, and Catalist works the same for AniList and MyAnimeList
users. Everything works offline too. Lists move in and out as MyAnimeList
XML, which both sites read.

There are two ways in: the tracker, a full-screen interface for the
terminal, and a command line made for daily tracking, scripts and pipes,
where every change can be undone and redone.

## Provider

Your **provider** is the site you use: AniList (the default) or
MyAnimeList. Its ids are the ones Catalist shows and accepts, and its name
sits in the pill at the bottom left. Only one is chosen at a time
(`catalist provider mal` to switch). It is kept with the lists, and is where
syncing with your account will plug in later (not done yet: lists are
local only).

---

## Quickstart

### Nix

```bash
nix run github:McKaigne/Catalist
# or, in a clone:
nix run
```

### CMake

You need a C++17 compiler, CMake 3.20 or later, Ninja or Make, and SQLite 3
(`libsqlite3-dev` on Debian and Ubuntu, `sqlite` elsewhere; without it
CMake downloads its source). FTXUI, doctest and fmt are used from the
system or downloaded by CMake. At run time, `curl` reaches AniList and
`img2sixel` (from libsixel) draws covers; without them Catalist works
offline and without covers.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/catalist
ctest --test-dir build --output-on-failure
sudo cmake --install build
```

The build also writes the manual page and the completions for zsh and
fish, and `cmake --install` puts them where `man` and the shells look,
with `cl`, a short name for `catalist`. When `dash`, `bash`, `zsh` or
`fish` is installed, `ctest` also runs the command line from that shell.

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

Whenever you change a title's status, rating or progress in the tracker,
Catalist asks AniList about it (once a run), filling in whichever id it
lacked: a series that has ended gets its final
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
| `a` | Add a title: type it, `Enter` looks it up on AniList, pick a match. The last row adds it by hand, which also works offline |
| `d` | Delete (asks first) |
| `q` | Quit |

Every change is saved the moment you make it.

## Command line

Run with a command, `catalist` works on the lists from the shell. `cl` is
the same program under a shorter name.

```sh
cl ls                              # every title, as a table
cl ls -s watching frieren          # one status, titles holding "frieren"
cl + frieren                       # one more episode or chapter
cl + -n 3 a:12                     # three more
cl set a:12 score=9 status=completed
cl done --score=8 taxi             # completed, with a score
cl add --pick=1 dandadan           # search AniList, take the first match
cl add --no-lookup "Some OVA"      # add by hand, offline
cl rm -f a:12                      # remove without asking
cl undo                            # take the last command back
cl redo                            # and make it again
cl search -m berserk               # look without adding
cl stats                           # the numbers of both lists
```

- **Refs:** every title has a ref, the first column of `ls`: `a:12` is an
  anime, `m:3` a manga. A command takes refs or any part of a title, in
  capitals or not; a title that is exactly the words given wins, and
  otherwise several matches are an error unless `--all` takes them all.
  `-a` and `-m` look in one list only. Refs are local and short: after
  `rm` of the title with the highest number, the next title added can get
  the same ref.
- **Options** follow POSIX: they come before the operands, group
  (`-amq`), take their value joined or apart (`-n3`, `-n 3`,
  `--sort=title`), and `--` ends them. Every command has `--help`, `cl
  help` lists them, and a mistyped command or option gets a suggestion.
- **Output:** a table at a terminal (in colour, unless `NO_COLOR` is set);
  into a pipe, one line a title with a tab between the fields, for `cut`,
  `awk`, `sort` and `read`; `--json` prints one JSON object a line, and
  `--header` names the columns. Messages and questions go to stderr. Exit
  codes: 0 done, 1 failed, 2 wrong command line.
- **Network:** only `add` and `search` go online. `add` fails when AniList
  cannot be reached, unless `--no-lookup` adds the title as written. The
  tracker keeps the totals current.
- **No terminal:** `catalist` alone opens the tracker; with no terminal
  (in a pipe, a script or cron) it prints its usage and exits with 2.

### Pipes

A lone `-` in place of the refs reads them from standard input, one a
line, so the refs `ls -q` prints can feed any command that changes titles:

```sh
cl ls -q -s watching | cl + -           # one more of everything watched
cl ls -q -s planned | head -n 3 | cl rm -f -
cl ls | awk -F '\t' '$5 >= 9 { print $3 }'   # titles scored 9 or 10
```

### Undo and redo

Every command that changes the lists is one step: `cl undo` takes the
last one back, `cl redo` makes it again, and `cl log` lists the last 100.
A step is taken back only when its titles are still as it left them;
otherwise the title is named and nothing changes, and `-f` takes it back
all the same. A new change after an undo ends what can be redone. Changes
made in the tracker are steps too.

Commands run with the same `CATALIST_TX` are one step, so one `cl undo`
reverts a whole loop. In sh, bash or zsh:

```sh
export CATALIST_TX=$(cl tx)
for ref in a:3 a:7 m:2; do
	cl + "$ref"
done
unset CATALIST_TX
cl undo        # all three at once
```

In fish:

```fish
set -lx CATALIST_TX (cl tx)
for ref in a:3 a:7 m:2
    cl + $ref
end
set -e CATALIST_TX
cl undo
```

### Completion

The completions for zsh and fish know the commands, their options and
values, and your titles: by ref, with the title beside it, or by any part
of a title. `cmake --install` and the Nix package put them in place for
`catalist` and `cl`. To set them up by hand:

```sh
catalist completion zsh > ~/.zfunc/_catalist     # ~/.zfunc in $fpath, before compinit
catalist completion fish > ~/.config/fish/completions/catalist.fish
```

## Importing and exporting

```bash
catalist import animelist.xml mangalist.xml   # MyAnimeList XML exports
catalist export anime > anime.xml             # MyAnimeList XML on stdout
catalist export manga > manga.xml
catalist export --format=csv anime > anime.csv   # for a spreadsheet
catalist export --format=json manga | jq .title  # one object a line
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
and only its totals are refreshed. Importing the same file twice adds
nothing, and an import is one step for undo. Titles without a MyAnimeList
id are left out of an XML export, with a note on stderr; the CSV and JSON
exports have every title, with the columns of `ls`.

---

## Data

- `~/.local/share/catalist/catalist.db` (or `$XDG_DATA_HOME/catalist`, or
  the directory in `CATALIST_DATA_DIR`): one SQLite file with both lists,
  the provider and the steps `undo` and `redo` can take. Each change is
  one SQLite transaction, so a crash never leaves half a change, and two
  programs working on the lists at once wait for each other. A save that
  fails is reported (the bottom bar in the tracker, exit code 1 on the
  command line).
- **Coming from 0.3.0 or earlier:** when there is no `catalist.db` yet,
  Catalist reads `anime.csv`, `manga.csv` and `settings` from
  `~/.config/catalist` (or `$XDG_CONFIG_HOME/catalist`, or the directory
  in `CATALIST_DATA_DIR`) into a new one, once, says so on stderr, and
  leaves the old files as they were. Files written by Catalist 0.2.1 to
  0.3.0 load, AniList ids included.
- For a spreadsheet, `catalist export --format=csv`.

The provider is the only setting. The theme is Catppuccin Mocha and the
tracker's list starts with new releases.

See [CHANGELOG.md](CHANGELOG.md) for release notes and
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for design decisions.

## License

MIT
