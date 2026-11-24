#!/bin/sh
# The command line as a script uses it, from a POSIX shell:
#
#   sh tests/smoke/smoke.sh build/catalist
#
# The lists are kept in a temporary directory, and a stub curl answers for
# AniList, so nothing outside is read, written or asked.

set -u

if [ "$#" -ne 1 ]; then
	printf 'usage: %s PROGRAM\n' "$0" >&2
	exit 2
fi
case "$1" in
/*) program=$1 ;;
*) program=$(pwd)/$1 ;;
esac
here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/catalist-smoke.XXXXXX") || exit 1
trap 'rm -rf "$work"' EXIT

CATALIST_DATA_DIR=$work/data
PATH=$here/stub:$PATH
NO_COLOR=1
export CATALIST_DATA_DIR PATH NO_COLOR

failures=0

fail() {
	printf 'FAIL: %s\n' "$1" >&2
	failures=$((failures + 1))
}

expect() {
	if [ "$2" != "$3" ]; then
		fail "$1: got '$2', wanted '$3'"
	fi
}

catalist() {
	"$program" "$@"
}

field() {
	catalist ls | awk -F '\t' -v ref="$1" -v n="$2" '$1 == ref { print $n }'
}

catalist >"$work/out" 2>"$work/err"
expect "no command: exit" "$?" 2
expect "no command: output" "$(cat "$work/out")" ""
grep -q 'no command given' "$work/err" || fail "no command: no note"

added=$(catalist add --pick=2 fullmetal)
expect "add --pick" "$added" "$(printf 'a:1\tFullmetal Alchemist\tadded\t-\tplanned')"
catalist add -q --status=watching "Odd Taxi"
catalist add -q --status=watching Dandadan
expect "add: the titles" "$(catalist ls -q | tr '\n' ' ')" "a:3 a:1 a:2 "

catalist ls -q -s watching | xargs "$program" + -q
expect "ls -q | xargs +: Odd Taxi" "$(field a:2 6)" 1
expect "ls -q | xargs +: Dandadan" "$(field a:3 6)" 1

catalist undo -q
expect "undo: Odd Taxi" "$(field a:2 6)" 0
expect "undo: Dandadan" "$(field a:3 6)" 0
catalist redo -q
expect "redo: Odd Taxi" "$(field a:2 6)" 1
expect "redo: Dandadan" "$(field a:3 6)" 1

catalist + -q a:2 a:3
catalist done -q a:1
expect "undo -n: before" "$(field a:2 6) $(field a:1 4)" "2 completed"
catalist undo -q -n 2
expect "undo -n 2: Odd Taxi" "$(field a:2 6)" 1
expect "undo -n 2: Dandadan" "$(field a:3 6)" 1
expect "undo -n 2: Fullmetal" "$(field a:1 4)" planned

catalist rm -f -q Dandadan
expect "rm -f" "$(catalist ls -q | tr '\n' ' ')" "a:1 a:2 "
catalist undo -q
expect "rm -f, undo" "$(field a:3 3)" Dandadan

catalist export anime 2>/dev/null |
	CATALIST_DATA_DIR=$work/other "$program" import -q -
expect "export | import" "$(CATALIST_DATA_DIR=$work/other "$program" ls | cut -f 1,3)" \
	"$(printf 'a:3\tDandadan\na:1\tFullmetal Alchemist\na:2\tOdd Taxi')"

catalist export anime >"$work/anime.xml" 2>/dev/null
mkfifo "$work/fifo"
cat "$work/anime.xml" >"$work/fifo" &
CATALIST_DATA_DIR=$work/piped "$program" import -q "$work/fifo"
expect "import from a pipe: exit" "$?" 0
wait
expect "import from a pipe" "$(CATALIST_DATA_DIR=$work/piped "$program" ls | cut -f 1,3)" \
	"$(printf 'a:3\tDandadan\na:1\tFullmetal Alchemist\na:2\tOdd Taxi')"

awk 'BEGIN {
	print "<myanimelist>"
	for (i = 1; i <= 3000; i++)
		printf "<anime><series_animedb_id>%d</series_animedb_id><series_title>Title %d</series_title><my_status>Watching</my_status></anime>\n", i, i
	print "</myanimelist>"
}' >"$work/many.xml"
CATALIST_DATA_DIR=$work/many "$program" import -q "$work/many.xml"
first=$(CATALIST_DATA_DIR=$work/many "$program" ls 2>"$work/err" | head -n 1 | cut -f 1)
expect "ls | head -n 1" "$first" a:1
expect "ls | head -n 1: standard error" "$(cat "$work/err")" ""

if [ -w /dev/full ]; then
	catalist export anime >/dev/full 2>"$work/err"
	expect "export > full disk: exit" "$?" 1
	grep -q 'could not write to standard output' "$work/err" ||
		fail "export > full disk: no message"
	catalist ls >/dev/full 2>/dev/null
	expect "ls > full disk: exit" "$?" 1
fi

if [ "$failures" -ne 0 ]; then
	printf '%s: %d failed\n' "$0" "$failures" >&2
	exit 1
fi
printf '%s: all passed\n' "$0"
