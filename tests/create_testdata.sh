#!/usr/bin/env bash
set -e
set -u

DATA_ROOT="${1:-}"
TRASH_ROOT="${2:-}"
RETENTION_WINDOW=${3:-}
FIXTURE_OPTION="${4:-}"
if [ -z "$DATA_ROOT" -o -z "$TRASH_ROOT" -o -z "$RETENTION_WINDOW" ]; then
	echo "*** ERROR *** usage: "$(basename "$0")" DATA_ROOT TRASH_ROOT RETENTION_WINDOW [FIXTURE_OPTION]" >&2
	exit 1
fi

case "$FIXTURE_OPTION" in
	""|--with-long-path|--with-trash-collision|--with-safety-failures)
		;;
	*)
		echo "*** ERROR *** unknown FIXTURE_OPTION [$FIXTURE_OPTION]" >&2
		exit 1
		;;
esac

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

AGE_KEEPME=$(( RETENTION_WINDOW * 9 / 10 ))  #9/10 the retention window
AGE_DELETEME=$(( RETENTION_WINDOW * 3 / 2 ))  #3/2 the retention window

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

ln -s foo.deleteme "$DATA_ROOT"/foo.symlink


#--- exempt data

for d in "$DATA_ROOT"/exempt "$DATA_ROOT"/subdir_a/exempt "$DATA_ROOT"/subdir_a/subdir_b/exempt; do
	mkdir "$d"
	touch --date="$D_DELETEME" "$d"/foo.keepme
done

mkdir "$DATA_ROOT"/exempt-sibling
touch --date="$D_DELETEME" "$DATA_ROOT"/exempt-sibling/prefix_bug.deleteme

#--- deep-directory test data
#
# this creates keepme/deleteme files in a deep directory hierarchy where the
# corresponding trash hierarchy does not yet exist.
BUG_REPRO_DIR="$DATA_ROOT"/mkdir_p_repro/level_1/level_2/level_3
mkdir -p "$BUG_REPRO_DIR"
touch --date="$D_KEEPME" "$BUG_REPRO_DIR"/mkdir_p_bug.keepme
touch --date="$D_DELETEME" "$BUG_REPRO_DIR"/mkdir_p_bug.deleteme

#--- long-path regression data

if [ "$FIXTURE_OPTION" = "--with-long-path" ]; then
	LONG_PATH_ROOT="$DATA_ROOT"/long_path_repro
	LONG_PATH_DELETE_FILE=long_path.deleteme
	LONG_PATH_KEEP_FILE=long_path.keepme
	LONG_PATH_COMPONENT_LEN=200
	PATH_MAX_VALUE=$(getconf PATH_MAX "$DATA_ROOT" 2>/dev/null || echo 4096)
	NAME_MAX_VALUE=$(getconf NAME_MAX "$DATA_ROOT" 2>/dev/null || echo 255)
	mkdir "$LONG_PATH_ROOT"
	CURRENT_LENGTH=$(cd "$LONG_PATH_ROOT" && printf '%s' "$PWD" | wc -c)
	DEPTH=0
	DATA_ROOT_ABS=$(cd "$DATA_ROOT" && printf '%s' "$PWD")
	TRASH_ROOT_PARENT_ABS=$(cd "$(dirname "$TRASH_ROOT")" && printf '%s' "$PWD")
	TRASH_ROOT_ABS="$TRASH_ROOT_PARENT_ABS/$(basename "$TRASH_ROOT")"
	TRASH_DELTA=$(( ${#TRASH_ROOT_ABS} - ${#DATA_ROOT_ABS} ))
	TARGET_FILE_PATH_LEN=$(( PATH_MAX_VALUE - TRASH_DELTA + 1 ))
	TARGET_DIR_LEN=$(( TARGET_FILE_PATH_LEN - 1 - ${#LONG_PATH_DELETE_FILE} ))
	if [ "$LONG_PATH_COMPONENT_LEN" -gt $(( NAME_MAX_VALUE - 16 )) ]; then
		LONG_PATH_COMPONENT_LEN=$(( NAME_MAX_VALUE - 16 ))
	fi
	LONG_PATH_COMPONENT=$(printf 'p%.0s' $(seq 1 "$LONG_PATH_COMPONENT_LEN"))

	(
		cd "$LONG_PATH_ROOT"
		while [ $(( CURRENT_LENGTH + 1 + 5 + LONG_PATH_COMPONENT_LEN )) -lt "$TARGET_DIR_LEN" ]; do
			COMPONENT=$(printf 'd%03d_%s' "$DEPTH" "$LONG_PATH_COMPONENT")
			mkdir "$COMPONENT"
			cd "$COMPONENT"
			CURRENT_LENGTH=$(( CURRENT_LENGTH + 1 + ${#COMPONENT} ))
			DEPTH=$(( DEPTH + 1 ))
		done

		FINAL_COMPONENT_LEN=$(( TARGET_DIR_LEN - CURRENT_LENGTH - 1 ))
		if [ "$FINAL_COMPONENT_LEN" -gt 0 ]; then
			FINAL_COMPONENT=$(printf 'q%.0s' $(seq 1 "$FINAL_COMPONENT_LEN"))
			mkdir "$FINAL_COMPONENT"
			cd "$FINAL_COMPONENT"
		fi

		touch --date="$D_KEEPME" "$LONG_PATH_KEEP_FILE"
		touch --date="$D_DELETEME" "$LONG_PATH_DELETE_FILE"
	)
fi

if [ "$FIXTURE_OPTION" = "--with-trash-collision" ]; then
	COLLISION_REPRO_DIR="$DATA_ROOT"/trash_collision_repro/child
	mkdir -p "$COLLISION_REPRO_DIR"
	touch --date="$D_KEEPME" "$COLLISION_REPRO_DIR"/collision_bug.keepme
	touch --date="$D_DELETEME" "$COLLISION_REPRO_DIR"/collision_bug.deleteme
fi

if [ "$FIXTURE_OPTION" = "--with-safety-failures" ]; then
	LEAF_COLLISION_REPRO_DIR="$DATA_ROOT"/leaf_collision_repro
	SYMLINK_TRAP_REPRO_DIR="$DATA_ROOT"/symlink_trap_repro
	mkdir -p "$LEAF_COLLISION_REPRO_DIR" "$SYMLINK_TRAP_REPRO_DIR"
	printf 'source\n' > "$LEAF_COLLISION_REPRO_DIR"/leaf_collision.deleteme
	touch --date="$D_DELETEME" "$LEAF_COLLISION_REPRO_DIR"/leaf_collision.deleteme
	printf 'source\n' > "$SYMLINK_TRAP_REPRO_DIR"/trapped.deleteme
	touch --date="$D_DELETEME" "$SYMLINK_TRAP_REPRO_DIR"/trapped.deleteme
	chmod 600 "$SYMLINK_TRAP_REPRO_DIR"/trapped.deleteme
fi


mkdir "$TRASH_ROOT"

if [ "$FIXTURE_OPTION" = "--with-trash-collision" ]; then
	touch "$TRASH_ROOT"/trash_collision_repro
fi

if [ "$FIXTURE_OPTION" = "--with-safety-failures" ]; then
	mkdir -p "$TRASH_ROOT"/leaf_collision_repro "$TRASH_ROOT"/symlink_trap_target
	printf 'trash\n' > "$TRASH_ROOT"/leaf_collision_repro/leaf_collision.deleteme
	touch --date="$D_DELETEME" "$TRASH_ROOT"/leaf_collision_repro/leaf_collision.deleteme
	ln -s symlink_trap_target "$TRASH_ROOT"/symlink_trap_repro
fi
