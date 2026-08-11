#!/bin/sh

set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
NOCT=${NOCT:-"$root/build-mt-debug/noct"}

if [ "$#" -ne 0 ]; then
    echo "usage: $0" >&2
    exit 2
fi
if [ ! -x "$NOCT" ]; then
    echo "Noct CLI not found: $NOCT" >&2
    exit 2
fi

case_dir="$root/tests/testcases"
cd "$case_dir"
log=$(mktemp)
trap 'rm -f -- noct-arch out "$log"' EXIT HUP INT TERM
"$NOCT" -j0 multiarch.noct | tee "$log"
if ! grep -Fx 'All available multi-architecture tests passed.' "$log" >/dev/null; then
    echo 'Multi-architecture tests failed.' >&2
    exit 1
fi
