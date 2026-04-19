#!/usr/bin/env bash
set -euo pipefail

TARGET="${1:-}"
if [ -z "$TARGET" ]; then
	echo "*** ERROR *** usage: $(basename "$0") TARGET" >&2
	exit 1
fi

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
export PATH="$ROOT_DIR/src:$PATH"
export MANPATH="$ROOT_DIR/man:${MANPATH:-}"

echo "== $(date -u +'%Y-%m-%dT%H:%M:%SZ') starting target: $TARGET =="

status=0
if ! timeout --signal=TERM --kill-after=30s 10m \
	make -C "$ROOT_DIR/tests" "$TARGET" EXTRA_MPIRUN_ARGS="--mca btl self,tcp --oversubscribe"; then
	status=$?
fi

echo "== $(date -u +'%Y-%m-%dT%H:%M:%SZ') finished target: $TARGET status=$status =="

if [ "$status" -ne 0 ]; then
	echo "== process snapshot after failure ==" >&2
	ps -ef >&2
	exit "$status"
fi
