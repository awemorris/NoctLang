#!/bin/sh

set -eu

case_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
root=$(CDPATH= cd -- "$case_dir/../.." && pwd)
NOCT=${NOCT:-"$root/build-static/noct"}
PARALLEL_EXPECT_SIMD=${PARALLEL_EXPECT_SIMD:-1}
tmp=${TMPDIR:-/tmp}/noct-parallel-analysis.$$

cleanup()
{
    rm -rf "$tmp"
}
trap cleanup EXIT HUP INT TERM
mkdir -p "$tmp"

echo 'Target-neutral loop analysis tests:'
for opt in 0 1 2 3; do
    NOCT_PARALLEL_DEBUG=1 LC_ALL=C "$NOCT" -j0 -O"$opt" \
        "$case_dir/parallel-analysis/basic.noct" \
        >"$tmp/stdout" 2>"$tmp/stderr"
    grep '^parallel-analysis ' "$tmp/stderr" >"$tmp/actual"
    diff "$case_dir/parallel-analysis/basic.noct.out" "$tmp/actual"
done

if [ "$PARALLEL_EXPECT_SIMD" -ne 0 ]; then
    echo 'SIMD/common-analysis integration:'
    NOCT_SIMD_ANALYSIS_COMPARE=1 LC_ALL=C "$NOCT" -j0 -O2 \
        "$case_dir/simd/blend.noct" >"$tmp/simd.stdout" 2>"$tmp/simd.stderr"
    grep 'SIMD-COMMON: .*blend.noct:11 legacy=accept common=accept reason=runtime-alias-guard .*alias-guards=1$' \
        "$tmp/simd.stderr" >/dev/null
    grep 'SIMD-COMMON: .*blend.noct:22 legacy=accept common=reject reason=scalar-carried ' \
        "$tmp/simd.stderr" >/dev/null
else
    echo 'SKIP SIMD/common-analysis integration (optimizer disabled)'
fi
echo 'PASS'
