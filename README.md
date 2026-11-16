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
links to a title's page there.

## Tracking

As Mihon tells its trackers, Catalist tells the sites you are logged in to
what changes in your lists, one way: your lists are the truth.

`catalist login anilist|mal|kitsu` logs in to a site; `catalist accounts`
lists the logins and `catalist logout SITE` forgets one. Each site has its
own way (AniList shows a token to paste back, MyAnimeList sends the browser
to an address to paste back, Kitsu takes your email and password once, for
a token). The tokens are kept apart from your lists, readable by you only,
in `~/.local/state/catalist/accounts`; passwords never are.

Logged in, each change to a title's status, score, progress, volumes,
rewatches or dates is sent at once to every such site where the title is
**linked** (has its id there). Titles added from a site, imported or
refreshed are linked to every site that knew their ids; `catalist link REF
SITE [ID]` links one by hand, `catalist ls --unlinked SITE` lists those that
are not, and `catalist unlink REF SITE` stops telling a site of one. What
could not be sent (offline, a site down, `--offline`, more than ten changes
at once) waits, and `catalist sync` sends it; `catalist sync --all` sends
every linked title, as after a first login. `rm` leaves the sites' lists
alone unless `--remote` is given.

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
fish, and `cmake --install` puts them where `man` and the shells look.
When `dash`, `bash` or `zsh` is installed, `ctest` also runs the command
line from that shell.

---

## Usage

```sh
catalist ls                           # every title, as a table
catalist ls -s watching frieren       # one status, titles holding "frieren"
catalist + frieren                    # one more episode or chapter
catalist + -n 3 a:12                  # three more
catalist set a:12 score=9 status=completed
catalist done --score=8 taxi          # completed, with a score
catalist add --pick=1 dandadan        # search AniList, take the first match
catalist add --no-lookup "Some OVA"   # add by hand, offline
catalist rm -f a:12                   # remove without asking
catalist undo                         # take the last command back
catalist redo                         # and make it again
catalist search -m berserk            # look without adding
catalist stats                        # the numbers of both lists
catalist help set                     # what a command takes
```

A shorter name is a shell alias away: `alias cl=catalist`.

- **Refs:** every title has a ref, the first column of `ls`: `a:12` is an
  anime, `m:3` a manga. A command takes refs or any part of a title, in
  capitals or not; a title that is exactly the words given wins, and
  otherwise several matches are an error unless `--all` takes them all.
  `-a` and `-m` look in one list only. Refs are local and short: after
  `rm` of the title with the highest number, the next title added can get
  the same ref.
- **Options** follow POSIX: they come before the operands, group
  (`-amq`), take their value joined or apart (`-n3`, `-n 3`,
  `--sort=title`), and `--` ends them. Every command has `--help`,
  `catalist help` lists them, and a mistyped command or option gets a suggestion.
- **Output:** a table at a terminal (in colour, unless `NO_COLOR` is set);
  into a pipe, one line a title with a tab between the fields, for `cut`,
  `awk`, `sort` and `read`; `--json` prints one JSON object a line, and
  `--header` names the columns. Messages and questions go to stderr. Exit
  codes: 0 done, 1 failed, 2 wrong command line (also `catalist` with no
  command).
- **Network:** only `add`, `search`, `refresh`, `import --user`, `login`
  and `sync` go online, and, logged in, the change a command saves is sent
  to the sites (see Tracking). `add` fails
  when the site cannot be reached, unless `--no-lookup` adds the title as
  written. A title's total, and how many episodes are out (which the `new`
  column counts from), are what the site said when it was added, until
  `refresh` asks again (without REFs, about everything you watch, read or
  have on hold; AniList answers fifty titles a request).

### Pipes

A lone `-` in place of the refs reads them from standard input, one a
line, so the refs `ls -q` prints can feed any command that changes titles:

```sh
catalist ls -q -s watching | catalist + -     # one more of everything watched
catalist ls -q -s planned | head -n 3 | catalist rm -f -
catalist ls | awk -F '\t' '$5 >= 9 { print $3 }'   # titles scored 9 or 10
```

### Undo and redo

Every command that changes the lists is one step: `catalist undo` takes
the last one back (`undo -n 3` the last three), `catalist redo` makes it
again, and `catalist log` lists the last 100. A step is taken back only
when its titles are still as it left them; otherwise the title is named
and nothing changes, and `-f` takes it back all the same. A new change
after an undo ends what can be redone.

### Completion

The completions for zsh and fish know the commands, their options and
values, and your titles: by ref, with the title beside it, or by any part
of a title. `cmake --install` and the Nix package put them in place. To
set them up by hand:

```sh
catalist completion zsh > ~/.zfunc/_catalist     # ~/.zfunc in $fpath, before compinit
catalist completion fish > ~/.config/fish/completions/catalist.fish
```

## Importing and exporting

```bash
catalist import --user Abel --from kitsu     # a user's public lists on a site
catalist import animelist.xml mangalist.xml   # MyAnimeList XML exports
catalist import ~/.config/catalist/anime.csv ~/.config/catalist/manga.csv   # from 0.3.0
catalist export anime > anime.xml             # MyAnimeList XML on stdout
catalist export manga > manga.xml
catalist export --format=csv anime > anime.csv   # for a spreadsheet
catalist export --format=json manga | jq .title  # one object a line
catalist export --format=json > backup.jsonl     # everything, ids and all
catalist import backup.jsonl                     # and back
```

- **From a site:** `import --user NAME` reads NAME's public lists on your
  provider, or the site `--from` names (AniList, MyAnimeList or Kitsu), the
  same way for each. The titles keep their ids on that site, so they come
  in linked to it (see Tracking).
- **From a file:** MyAnimeList's export page gives `.xml.gz` files: unpack
  them (`gunzip`) and import them.
- **To a site:** log in and `catalist sync --all`; or upload what
  `catalist export` writes on the import page of MyAnimeList, AniList or
  Kitsu, which all read MyAnimeList XML.

Importing never overwrites what you track: a title already in the catalogue
(the same AniList, MyAnimeList or Kitsu id) keeps your score, progress and dates,
and only its totals are refreshed. Importing the same file twice adds
nothing, and an import is one step for undo. Titles without a MyAnimeList
id are left out of an XML export, with a note on stderr; the CSV and JSON
exports have every title, with the columns of `show` (every site's id among
them), and `import` reads the JSON back whole: it is the backup.

---

## Data

- `~/.local/share/catalist/catalist.db` (or `$XDG_DATA_HOME/catalist`, or
  the directory in `CATALIST_DATA_DIR`): one SQLite file with both lists,
  the provider and the steps `undo` and `redo` can take. Each change is
  one SQLite transaction, so a crash never leaves half a change, and two
  programs working on the lists at once wait for each other. A save that
  fails is reported with exit code 1.
- **Coming from 0.3.0 or earlier:** `catalist import
  ~/.config/catalist/anime.csv ~/.config/catalist/manga.csv` reads the old
  lists in, AniList ids included, and leaves the files as they were;
  `catalist provider` sets the provider again.
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
