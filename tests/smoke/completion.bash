# Completes command lines as bash would, with the script that
# `catalist completion bash` wrote, against the built program and a list of
# its own. Prints what went wrong; exits 1 when anything did.
#
#   bash tests/smoke/completion.bash build/share/bash/catalist build/catalist

set -u
script=$1
# Absolute, as the link to it below must be.
program=$(cd "$(dirname "$2")" && pwd)/$(basename "$2")

# A bash built without readline (Nix's plain bash) has no programmable
# completion to test: 77 tells ctest the test was skipped.
if ! type -t compgen >/dev/null; then
	echo "skipped: this bash has no compgen; run it with bash-interactive"
	exit 77
fi

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
export CATALIST_DATA_DIR="$work/data"
mkdir "$work/bin"
ln -s "$program" "$work/bin/catalist"
PATH="$work/bin:$PATH"

catalist add -q --no-lookup --status=watching "Odd Taxi"
catalist add -q --no-lookup "Sousou no Frieren"

# shellcheck source=/dev/null
source "$script"

# What bash offers for the line, the cursor at its end, one a line.
offered() {
	COMP_LINE=$1
	COMP_POINT=${#COMP_LINE}
	# bash's own words: apart at spaces, and at : and = as well.
	local parts word piece
	read -ra parts <<<"$COMP_LINE"
	[[ $COMP_LINE == *' ' ]] && parts+=("")
	COMP_WORDS=()
	for word in "${parts[@]}"; do
		[[ -z $word ]] && COMP_WORDS+=("")
		while [[ -n $word ]]; do
			if [[ $word == [:=]* ]]; then
				piece=${word%%[!:=]*}
			else
				piece=${word%%[:=]*}
			fi
			COMP_WORDS+=("$piece")
			word=${word#"$piece"}
		done
	done
	COMP_CWORD=$((${#COMP_WORDS[@]} - 1))
	COMPREPLY=()
	_catalist
	printf '%s\n' "${COMPREPLY[@]}"
}

failed=0
expect() {
	local got
	got=$(offered "$2")
	if [[ $got != "$3" ]]; then
		printf 'FAIL: %s\n  line:     %s\n  offered:  %s\n  expected: %s\n' "$1" "$2" \
			"${got//$'\n'/ | }" "${3//$'\n'/ | }"
		failed=1
	fi
}

expect "the commands" "catalist se" $'set\nsearch'
expect "the program's options" "catalist --co" "--color="
expect "a value of the program's" "catalist --color=al" "always"
expect "the command after the program's options" "catalist -t --color never l" $'ls\nlogin\nlogout\nlog'
expect "a command's options" "catalist ls --so" "--sort="
expect "a value apart" "catalist ls -s on" "on-hold"
expect "a value joined" "catalist ls --sort=sc" "score"
expect "the refs" "catalist show a:" $':1\n:2'
expect "a title, in capitals or not" "catalist + odd" 'Odd\ Taxi'
expect "a title after the program's options" "catalist -t show sou" 'Sousou\ no\ Frieren'
expect "the - command" "catalist - fri" ""
expect "the fields after the ref" "catalist set a:1 sc" "score="
expect "the statuses of status=" "catalist set a:1 status=on" "on-hold"
expect "the words of a command" "catalist completion b" "bash"
expect "the commands of help" "catalist help st" "stats"
expect "nothing for text" "catalist search fri" ""

exit $failed
