#!/usr/bin/env bash
set -e
set -u

if ! which fscull &>/dev/null; then
	echo 'fscull: command not found; maybe try:' 2>&1
	echo '    export PATH=$PWD/../src:$PATH' 2>&1
	exit 1
fi
