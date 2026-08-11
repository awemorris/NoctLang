#!/bin/sh

# BeUI host tests.  The core suite links the built API library; the
# PC-98 backend suites compile their drivers directly, because those
# sources only enter the library on a PC-98 target build.

set -eu

root=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
build_dir=${1:-"$root/build-static"}
case "$build_dir" in
/*) ;;
*) build_dir="$root/$build_dir" ;;
esac
cc=${CC:-cc}
api="$root/src/api"

test -f "$build_dir/libnoctapi.a" || {
	echo "Noct API static library not found: $build_dir/libnoctapi.a" >&2
	echo "Configure with -DNOCT_ENABLE_API_BEUI=ON and build first." >&2
	exit 1
}

"$cc" -I"$root/include" "$root/tests/testcases/beui-test.c" \
	"$build_dir/libnoctapi.a" "$build_dir/libnoct.a" -lm \
	-o "$build_dir/noct-beui-test"
"$build_dir/noct-beui-test"

"$cc" -I"$root/include" -I"$api" "$root/tests/testcases/beui-pc98-gdc-test.c" \
	"$api/beui-pc98-gdc.c" "$api/beui-core.c" "$api/beui-image.c" \
	-o "$build_dir/noct-beui-pc98-gdc-test"
"$build_dir/noct-beui-pc98-gdc-test"
echo 'BeUI PC-98 GDC tests: OK'

"$cc" -I"$root/include" -I"$api" "$root/tests/testcases/beui-pc98-cirrus-test.c" \
	"$api/beui-pc98-cirrus.c" \
	-o "$build_dir/noct-beui-pc98-cirrus-test"
"$build_dir/noct-beui-pc98-cirrus-test"
echo 'BeUI PC-98 Cirrus tests: OK'
