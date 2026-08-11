#!/bin/sh

set -eu

root=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
build_dir=${1:-"$root/build-static"}
case "$build_dir" in
/*) ;;
*) build_dir="$root/$build_dir" ;;
esac
cc=${CC:-cc}
test_bin="$build_dir/noct-api-backend-test"

test -f "$build_dir/libnoct.a" || {
	echo "Noct static library not found: $build_dir/libnoct.a" >&2
	exit 1
}
test -f "$build_dir/libnoctapi.a" || {
	echo "Noct API static library not found: $build_dir/libnoctapi.a" >&2
	exit 1
}

"$cc" -I"$root/include" "$root/tests/testcases/api-backend-test.c" \
	"$build_dir/libnoctapi.a" "$build_dir/libnoct.a" -lm -o "$test_bin"
"$test_bin"
