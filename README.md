# Catalist

A small, local-first anime and manga tracker for the command line, written
in **C++17**. It is made for daily tracking, scripts and pipes, and every
change can be undone and redone.

Your lists live on your computer in one SQLite file. Catalist goes online
only when you add, search for or refresh a title, through the public API of
[AniList](https://anilist.co), [MyAnimeList](https://myanimelist.net) or
[Kitsu](https://kitsu.app). The three work the same way, and each entry
keeps its id on every site that is known, so Catalist serves users of all
three. Everything else works offline. Lists move in and out as MyAnimeList
XML, which all three sites read.

The full-screen terminal interface of earlier versions is now a separate
program, Catalist-TUI. Both keep their lists in the same `catalist.db`, so
they can be used side by side.

## Provider

Your **provider** is the site you use: AniList (the default), MyAnimeList
or Kitsu (`catalist provider kitsu` to switch). `search` and `add` ask it,
unless `--from SITE` names another for one command, and `catalist show`
links to a title's page there. Telling the sites what changes in your
lists, as Mihon tells its trackers, will come later: for now the lists are
local only.

`catalist login anilist|mal|kitsu` logs in to a site; `catalist accounts`
lists the logins and `catalist logout SITE` forgets one. Each site has its
own way (AniList shows a token to paste back, MyAnimeList sends the browser
to an address to paste back, Kitsu takes your email and password once, for
a token). The tokens are kept apart from your lists, readable by you only,
in `~/.local/state/catalist/accounts`; passwords never are.

MyAnimeList's API answers only programs it has registered, even about
public titles. Register Catalist once at
<https://myanimelist.net/apiconfig> and build with
`-DCATALIST_MAL_CLIENT_ID=...`, or set `CATALIST_MAL_CLIENT_ID`. Logging
in to AniList or Kitsu likewise wants their ids
(`CATALIST_ANILIST_CLIENT_ID`; `CATALIST_KITSU_CLIENT_ID` and
`CATALIST_KITSU_CLIENT_SECRET`).

---

## Quickstart

### Nix

```bash
nix run github:McKaigne/Catalist -- help
# or, in a clone:
nix run . -- help
```

### CMake

You need a C++17 compiler, CMake 3.20 or later, Ninja or Make, and SQLite 3
(`libsqlite3-dev` on Debian and Ubuntu, `sqlite` elsewhere; without it
CMake downloads its source). doctest and fmt are used from the system or
downloaded by CMake. At run time, `curl` reaches AniList; without it
`add --no-lookup` still works.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/catalist help
ctest --test-dir build --output-on-failure
sudo cmake --install build
```

The build also writes the manual page and the completions for zsh and
fish, and `cmake --install` puts them where `man` and the shells look,
with `cl`, a short name for `catalist`. When `dash`, `bash`, `zsh` or
`fish` is installed, `ctest` also runs the command line from that shell.

---

## Usage

`cl` is `catalist` under a shorter name.

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
cl help set                        # what a command takes
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
  codes: 0 done, 1 failed, 2 wrong command line (also `catalist` with no
  command).
- **Network:** only `add`, `search` and `refresh` go online. `add` fails
  when the site cannot be reached, unless `--no-lookup` adds the title as
  written. A title's total, and how many episodes are out (which the `new`
  column counts from), are what the site said when it was added, until
  `refresh` asks again (without REFs, about everything you watch, read or
  have on hold; AniList answers fifty titles a request).

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
all the same. A new change after an undo ends what can be redone.

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
  fails is reported with exit code 1.
- **Coming from 0.3.0 or earlier:** when there is no `catalist.db` yet,
  Catalist reads `anime.csv`, `manga.csv` and `settings` from
  `~/.config/catalist` (or `$XDG_CONFIG_HOME/catalist`, or the directory
  in `CATALIST_DATA_DIR`) into a new one, once, says so on stderr, and
  leaves the old files as they were. Files written by Catalist 0.2.1 to
  0.3.0 load, AniList ids included.
- For a spreadsheet, `catalist export --format=csv`.

The provider is the only setting.

## Source layout

```
src/main.cpp      reads the environment and runs the command line
src/system.*      everything that touches the outside world: files, curl, the clock, the terminal
src/core/         a title and its rules, dates, results, UTF-8 text (no I/O)
src/storage/      the lists in SQLite, the undo history, the old CSV files, MyAnimeList XML
src/web/          AniList, MyAnimeList and Kitsu behind one tracker interface, and the JSON they answer in
src/cli/          the command line: parser, refs, output, completion, and the commands
                  (listing, tracking, online, transfer, history, setup)
tests/            unit tests, one folder per source folder; tests/smoke runs the program from the shells
docs/             the design decisions and the manual page's template
cmake/            the build step that writes the manual page and the completions
```

See [CHANGELOG.md](CHANGELOG.md) for release notes and
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for design decisions.

## License

MIT
