#!/bin/sh

set -eu

root=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
build_dir=${1:-"$root/build-debug"}
case "$build_dir" in
/*) ;;
*) build_dir="$root/$build_dir" ;;
esac
cc=${CC:-cc}
out="$build_dir/jit-slab-test"

"$cc" -I"$root/include" "$root/tests/testcases/jit-slab-test.c" \
	"$build_dir/libnoct.a" -lm -lpthread -o "$out"
"$out"

# On x86_64 O3 the first function plus blend exceed 28KiB, while blend alone
# fits. This exercises whole-function retry on a fresh slab.
"$build_dir/noct" -O3 --jit-code-size=28672 \
	"$root/tests/testcases/simd/blend2.noct" > "$build_dir/jit-slab-blend.out"
diff "$root/tests/testcases/simd/blend2.noct.out" "$build_dir/jit-slab-blend.out"
