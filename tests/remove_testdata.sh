#!/usr/bin/env bash
set -e
set -u

DATA_ROOT="${1:-}"
TRASH_ROOT="${2:-}"
if [ -z "$DATA_ROOT" -o -z "$TRASH_ROOT" ]; then
	echo "*** ERROR *** usage: "$(basename "$0")" DATA_ROOT TRASH_ROOT" >&2
	exit 1
fi


#---


rm -fr ./"$DATA_ROOT" 
rm -fr ./"$TRASH_ROOT"
