#!/bin/sh

set -eu

case_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
root=$(CDPATH= cd -- "$case_dir/../.." && pwd)
build_arg=${1:-build-accel-vulkan}

case "$build_arg" in
/*)
	build_dir=$build_arg
	;;
*)
	build_dir=$root/$build_arg
	;;
esac

tmp_root=${TMPDIR:-"$build_dir/test-tmp"}
noct=$build_dir/noct

mkdir -p "$tmp_root"
work=$(mktemp -d "$tmp_root/gpu-vulkan.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

${CMAKE:-cmake} --build "$build_dir" --target noctcli

if test -n "${NOCT_GPU_NAME:-}"; then
	gpu_option="--gpu=$NOCT_GPU_NAME"
else
	gpu_option=--gpu
fi

first=$root/tests/testcases/gpu-vulkan/basic-int32.noct
if ! "$noct" -j0 -O1 "$gpu_option" "$first" \
	> "$work/probe.out" 2> "$work/probe.err"; then
	if grep -E 'Vulkan 1\.2|Vulkan device|compute device|Vulkan loader' \
		"$work/probe.out" "$work/probe.err" >/dev/null; then
		echo 'SKIP: no suitable Vulkan 1.2 compute device is available.'
		exit 0
	fi
	cat "$work/probe.out" "$work/probe.err" >&2
	exit 1
fi
diff -u "$first.out" "$work/probe.out"

for case_name in basic-int32 basic-uint32 basic-float \
		 two-kernel partial-write zero-trip; do
	source=$root/tests/testcases/gpu-vulkan/$case_name.noct
	expected=$source.out

	"$noct" -j0 -O0 "$gpu_option" "$source" > "$work/$case_name-o0.out"
	diff -u "$expected" "$work/$case_name-o0.out"

	"$noct" -j0 -O1 "$gpu_option" "$source" > "$work/$case_name-j0.out"
	diff -u "$expected" "$work/$case_name-j0.out"

	"$noct" -j -O1 "$gpu_option" "$source" > "$work/$case_name-jit.out"
	diff -u "$expected" "$work/$case_name-jit.out"
done

for case_name in cpu-backed; do
	source=$root/tests/testcases/gpu-local/$case_name.noct
	expected=$source.out

	"$noct" -j0 -O0 "$gpu_option" "$source" > "$work/local-$case_name-o0.out"
	diff -u "$expected" "$work/local-$case_name-o0.out"

	"$noct" -j0 -O1 "$gpu_option" "$source" > "$work/local-$case_name-j0.out"
	diff -u "$expected" "$work/local-$case_name-j0.out"

	"$noct" -j -O1 "$gpu_option" "$source" > "$work/local-$case_name-jit.out"
	diff -u "$expected" "$work/local-$case_name-jit.out"
done

if "$noct" -j0 -O1 --gpu=Noct-Missing-GPU "$first" \
	> "$work/missing.out" 2>&1; then
	echo 'Missing exact Vulkan device name was accepted.' >&2
	exit 1
fi
grep -F "Vulkan device 'Noct-Missing-GPU' was not found." \
	"$work/missing.out" >/dev/null

echo 'Vulkan hardware execution tests passed.'
