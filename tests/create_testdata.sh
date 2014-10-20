#!/usr/bin/env bash
set -e
set -u

DATA_ROOT="${1:-}"
TRASH_ROOT="${2:-}"
if [ -z "$DATA_ROOT" -o -z "$TRASH_ROOT" ]; then
	echo "*** ERROR *** usage: "$(basename "$0")" DATA_ROOT TRASH_ROOT" >&2
	exit 1
fi

source settings.sh


#---


if [ -d "$DATA_ROOT" ]; then
	echo "*** ERROR *** \$DATA_ROOT directory [$DATA_ROOT] already exists; run \`make clean\` first" >&2
	exit 1
fi
if [ -d "$TRASH_ROOT" ]; then
	echo "*** ERROR *** \$TRASH_ROOT directory [$TRASH_ROOT] already exists; run \`make clean\` first" >&2
	exit 1
fi

for d in "$DATA_ROOT" "$DATA_ROOT"/subdir_a "$DATA_ROOT"/subdir_a/subdir_b; do
	mkdir "$d"
	for f in \
		foo \
		bar \
		"filename with spaces" \
		; do
		touch --date="$D_KEEPME" "$d/$f".keepme
		touch --date="$D_DELETEME" "$d/$f".deleteme
	done
done

mkdir "$TRASH_ROOT"
