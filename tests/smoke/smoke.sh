#!/bin/sh
# Runs the real program from a POSIX shell, on a temporary list. A stub curl
# answers for AniList.
#
#   sh tests/smoke/smoke.sh build/catalist

here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
program=$1
CATALIST_CSV=$work/catalist.csv
export CATALIST_CSV
PATH="$here/stub:$PATH"
export PATH
failures=0

fail() {
	echo "FAIL: $1" >&2
	failures=$((failures + 1))
}

menu() {
	printf "$1" | "$program"
}

has() {
	case "$2" in
	*"$3"*) ;;
	*) fail "$1: no '$3'" ;;
	esac
}

"$program" ls >/dev/null 2>"$work/err"
[ "$?" -eq 2 ] || fail "an argument is refused"
grep -q 'takes no arguments' "$work/err" || fail "an argument: no note"

shown=$(menu '0\n')
[ "$?" -eq 0 ] || fail "exit code"
has "first run" "$shown" "Add Title"
has "first run" "$shown" "Goodbye!"
[ ! -e "$work/catalist.csv" ] || fail "looking wrote a file"

shown=$(menu '1\n1\nfullmetal\n2\n1\n\n0\n')
has "add" "$shown" "Fullmetal Alchemist added as Plan to Watch."
head -n 1 "$work/catalist.csv" | grep -q '^id,type,title,' || fail "csv header"
grep -q '^1,anime,Fullmetal Alchemist,' "$work/catalist.csv" || fail "csv row"

menu '1\n1\nOdd Taxi\n2\n\n0\n' >/dev/null
shown=$(menu '3\n1\n\n0\n')
arrow=$(printf '\363\260\263\237')
has "count" "$shown" "progress 0 $arrow 1/13"
shown=$(menu '0\n')
has "reopened" "$shown" "2 anime"
has "reopened" "$shown" "1 in progress"

grep -q ',Odd Taxi,,watching,0,1,' "$work/catalist.csv" || fail "count reached the file"

shown=$(menu "7\n2\n$work/anime.xml\n\n0\n0\n")
has "export" "$shown" "Saved $work/anime.xml."
rm "$work/catalist.csv"
shown=$(menu "7\n1\n$work/anime.xml\n\n0\n0\n")
has "import" "$shown" "2 titles, 2 added"

printf 'damaged\n' >"$work/catalist.csv"
menu '0\n' >/dev/null 2>"$work/err"
[ "$?" -eq 1 ] || fail "a damaged file stops the program"
[ "$(cat "$work/catalist.csv")" = "damaged" ] || fail "a damaged file was overwritten"

if [ "$failures" -gt 0 ]; then
	echo "$0: $failures failed" >&2
	exit 1
fi
