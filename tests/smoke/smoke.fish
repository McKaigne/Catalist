# The command line as a script uses it, from fish; smoke.sh does the same
# from a POSIX shell.
#
#   fish tests/smoke/smoke.fish build/catalist
#
# The lists are kept in a temporary directory, and a stub curl answers for
# AniList, so nothing outside is read, written or asked.

if test (count $argv) -ne 1
    printf 'usage: %s PROGRAM\n' (status filename) >&2
    exit 2
end
set -g program (path resolve -- $argv[1])
set -l here (path dirname -- (path resolve -- (status filename)))
set -q TMPDIR; or set -l TMPDIR /tmp
set -g work (mktemp -d "$TMPDIR/catalist-smoke.XXXXXX"); or exit 1
function __smoke_clean_up --on-event fish_exit
    rm -rf $work
end

set -gx CATALIST_DATA_DIR $work/data
set -gx PATH $here/stub $PATH
set -gx NO_COLOR 1
set -e CATALIST_TX

set -g failures 0

function fail
    printf 'FAIL: %s\n' $argv[1] >&2
    set failures (math $failures + 1)
end

# expect WHAT GOT WANTED
function expect
    if test "$argv[2]" != "$argv[3]"
        fail "$argv[1]: got '$argv[2]', wanted '$argv[3]'"
    end
end

function catalist
    $program $argv
end

# The field of `ls` numbered $argv[2] (6 is the progress, 4 the status) for
# the ref $argv[1].
function field
    catalist ls | awk -F '\t' -v ref=$argv[1] -v n=$argv[2] '$1 == ref { print $n }'
end

# Without a command there is nothing to do.
catalist >$work/out 2>$work/err
expect "no command: exit" $status 2
expect "no command: output" "$(cat $work/out)" ""
grep -q 'no command given' $work/err; or fail "no command: no note"

# add --pick takes one of AniList's matches; --no-lookup stays offline.
expect "add --pick" "$(catalist add --pick=2 fullmetal)" \
    "$(printf 'a:1\tFullmetal Alchemist\tadded\t-\tplanned')"
catalist add -q --no-lookup --status=watching "Odd Taxi"
catalist add -q --no-lookup --status=watching Dandadan
expect "add: the titles" "$(catalist ls -q | string join ' ')" "a:3 a:1 a:2"

# The refs of one command feed another.
catalist ls -q -s watching | catalist + -q -
expect "ls -q | + -: Odd Taxi" "$(field a:2 6)" 1
expect "ls -q | + -: Dandadan" "$(field a:3 6)" 1

# undo takes the whole command back; redo makes it again.
catalist undo -q
expect "undo: Odd Taxi" "$(field a:2 6)" 0
expect "undo: Dandadan" "$(field a:3 6)" 0
catalist redo -q
expect "redo: Odd Taxi" "$(field a:2 6)" 1
expect "redo: Dandadan" "$(field a:3 6)" 1

# Commands run under one CATALIST_TX are one step for undo.
set -lx CATALIST_TX (catalist tx)
for ref in a:2 a:3
    catalist + -q $ref
end
catalist done -q a:1
set -e CATALIST_TX
expect "CATALIST_TX: before undo" "$(field a:2 6) $(field a:1 4)" "2 completed"
catalist undo -q
expect "CATALIST_TX: one undo, Odd Taxi" "$(field a:2 6)" 1
expect "CATALIST_TX: one undo, Dandadan" "$(field a:3 6)" 1
expect "CATALIST_TX: one undo, Fullmetal" "$(field a:1 4)" planned

# rm -f asks nobody; undo brings the title back under its ref.
catalist rm -f -q Dandadan
expect "rm -f" "$(catalist ls -q | string join ' ')" "a:1 a:2"
catalist undo -q
expect "rm -f, undo" "$(field a:3 3)" Dandadan

# export | import: what one list holds, another reads. The titles added by
# hand have no MyAnimeList id, so XML leaves them out.
catalist export anime 2>/dev/null | CATALIST_DATA_DIR=$work/other $program import -q -
expect "export | import" "$(CATALIST_DATA_DIR=$work/other $program ls | cut -f 1,3)" \
    "$(printf 'a:1\tFullmetal Alchemist')"

# A reader that stops early is no error.
awk 'BEGIN {
	print "<myanimelist>"
	for (i = 1; i <= 3000; i++)
		printf "<anime><series_animedb_id>%d</series_animedb_id><series_title>Title %d</series_title><my_status>Watching</my_status></anime>\n", i, i
	print "</myanimelist>"
}' >$work/many.xml
CATALIST_DATA_DIR=$work/many $program import -q $work/many.xml
set -l first (CATALIST_DATA_DIR=$work/many $program ls 2>$work/err | head -n 1 | cut -f 1)
expect "ls | head -n 1" "$first" a:1
expect "ls | head -n 1: standard error" "$(cat $work/err)" ""

# The completions ask for the refs; with no lists there are none, quietly.
expect "__complete refs" "$(catalist __complete refs | head -n 1)" \
    "$(printf 'a:1\tFullmetal Alchemist')"
CATALIST_DATA_DIR=$work/none $program __complete refs >$work/out 2>&1
expect "__complete, no lists: exit" $status 0
expect "__complete, no lists" "$(cat $work/out)" ""
test -e $work/none; and fail "__complete made a data directory"

if test $failures -ne 0
    printf '%s: %d failed\n' (status filename) $failures >&2
    exit 1
end
printf '%s: all passed\n' (status filename)
