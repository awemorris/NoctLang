#!/bin/sh

set -eu

root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
build_dir=${1:-"$root/build-debug"}
cc=${CC:-cc}
out="$build_dir/fma-helper-test"

"$cc" -I"$root/include" -I"$root/src/core" \
    "$root/tests/fma-helper-test.c" \
    "$build_dir/libnoctapi.a" "$build_dir/libnoct.a" -lm -o "$out"
"$out"
