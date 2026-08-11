#!/bin/sh

set -eu

root=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
build_dir=${1:-"$root/build-debug"}
case "$build_dir" in
/*) ;;
*) build_dir="$root/$build_dir" ;;
esac
cc=${CC:-cc}
out="$build_dir/fma-helper-test"

"$cc" -I"$root/include" -I"$root/src/core" \
    "$root/tests/testcases/fma-helper-test.c" \
    "$build_dir/libnoctapi.a" "$build_dir/libnoct.a" -lm -o "$out"
"$out"
