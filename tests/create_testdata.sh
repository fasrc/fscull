#!/usr/bin/env bash
set -e
set -u

DATA_ROOT="${1:-}"
TRASH_ROOT="${2:-}"
RETENTION_WINDOW=${3:-}
if [ -z "$DATA_ROOT" -o -z "$TRASH_ROOT" -o -z "$RETENTION_WINDOW" ]; then
	echo "*** ERROR *** usage: "$(basename "$0")" DATA_ROOT TRASH_ROOT RETENTION_WINDOW" >&2
	exit 1
fi

if [ -d "$DATA_ROOT" ]; then
	echo "*** ERROR *** \$DATA_ROOT directory [$DATA_ROOT] already exists; run \`make clean\` first" >&2
	exit 1
fi
if [ -d "$TRASH_ROOT" ]; then
	echo "*** ERROR *** \$TRASH_ROOT directory [$TRASH_ROOT] already exists; run \`make clean\` first" >&2
	exit 1
fi


#---


#--- time calculations

AGE_KEEPME=$(( RETENTION_WINDOW / 2 ))  #1/2 the retention window
AGE_DELETEME=$(( RETENTION_WINDOW / 2 * 3 ))  #3/2 the retention window

#timestamps in seconds since the epoch
T_NOW=$(date +%s)
T_KEEPME=$(( T_NOW - AGE_KEEPME ))
T_DELETEME=$(( T_NOW - AGE_DELETEME ))

#dates in a human readable form that touch also understands
D_NOW=$(     date -d @$T_NOW      +'%Y-%m-%d %H:%M:%S')
D_KEEPME=$(  date -d @$T_KEEPME   +'%Y-%m-%d %H:%M:%S')
D_DELETEME=$(date -d @$T_DELETEME +'%Y-%m-%d %H:%M:%S')


#--- basic data

for d in "$DATA_ROOT" "$DATA_ROOT"/subdir_a "$DATA_ROOT"/subdir_a/subdir_b; do
	mkdir "$d"
	
	#--- basic data
	for f in \
		foo \
		bar \
		"filename with spaces" \
		; do
		touch --date="$D_KEEPME" "$d/$f".keepme
		touch --date="$D_DELETEME" "$d/$f".deleteme
	done
done


#--- exempt data

for d in "$DATA_ROOT"/exempt "$DATA_ROOT"/subdir_a/exempt "$DATA_ROOT"/subdir_a/subdir_b/exempt; do
	mkdir "$d"
	touch --date="$D_DELETEME" "$d"/foo.keepme
done


mkdir "$TRASH_ROOT"
