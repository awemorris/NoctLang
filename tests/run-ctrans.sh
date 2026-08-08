#!/bin/sh

set -eu

root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
build_dir=${1:-"$root/build-static"}
cc=${CC:-cc}
noct="$build_dir/noct"
work_dir="$build_dir/ctrans-test"

test -x "$noct" || {
	echo "Noct CLI not found: $noct" >&2
	exit 1
}
test -f "$build_dir/libnoct.a" || {
	echo "Noct static library not found: $build_dir/libnoct.a" >&2
	exit 1
}
test -f "$build_dir/libnoctapi.a" || {
	echo "Noct API static library not found: $build_dir/libnoctapi.a" >&2
	exit 1
}

mkdir -p "$work_dir"

# Each case runs at optimize level 0 and 2 (level 2 enables the ABCE
# pass, whose OP_PLOAD8U/OP_PSTORE8 output is C-specific) against the
# same golden output.
for level in 0 2; do
	echo "(--optimize-level=$level)"
	for tc in "$root"/tests/ctrans/*.noct; do
		name=$(basename "$tc" .noct)-O$level
		echo "$tc"

		# Translate to C, compile standalone against the public
		# headers, then run and compare with the golden output.
		"$noct" --ansic --optimize-level=$level \
			"$work_dir/$name.c" "$tc"
		"$cc" -I"$root/include" "$root/tests/ctrans-test.c" \
			"$work_dir/$name.c" \
			"$build_dir/libnoctapi.a" "$build_dir/libnoct.a" -lm \
			-o "$work_dir/$name"
		"$work_dir/$name" > "$work_dir/$name.out"
		diff "$tc.out" "$work_dir/$name.out"
	done
done

echo 'All ctrans tests passed.'
