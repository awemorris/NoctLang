#!/bin/sh

set -eu

NOCT=${NOCT:-../../build-mt-debug/noct}
work=".tmp-cli-options-$$"
mkdir "$work"
trap 'rm -rf "$work"' EXIT HUP INT TERM

echo 'CLI optimization-level tests:'

for option in -O -O0 -O1 -O2 -O3 -O9; do
    $NOCT "$option" -j0 simd/f32.noct > "$work/out"
    diff simd/f32.noct.out "$work/out"
done

# Compile-mode option order is observable through O3-only FMA metadata.
cp simd/drawimage/blend-alpha.noct "$work/blend-alpha.noct"
$NOCT --compile -O3 -O2 "$work/blend-alpha.noct"
if grep -a -q '^FMA Ops$' "$work/blend-alpha.nb"; then
    echo 'last-option-wins failed for -O3 -O2' >&2
    exit 1
fi
$NOCT --compile -O2 -O3 "$work/blend-alpha.noct"
grep -a -q '^FMA Ops$' "$work/blend-alpha.nb"

$NOCT --ansic -O3 "$work/f32.c" simd/f32.noct
test -s "$work/f32.c"

# -j is eager: even a once-called entry point is compiled while registering.
NOCT_JIT_DEBUG=1 $NOCT -j -O0 simd/f32.noct \
    > "$work/out" 2> "$work/jit-debug"
grep -q '^noct-jit: .*: compiled$' "$work/jit-debug"
NOCT_JIT_DEBUG=1 $NOCT -j0 -O0 simd/f32.noct \
    > "$work/out" 2> "$work/no-jit-debug"
test ! -s "$work/no-jit-debug"

# Bare -O keeps LINEINFO, while the numbered optimized preset drops it.
$NOCT -j0 -O typing/anno_violate.noct \
    > "$work/lineinfo" 2>&1 || true
grep -q ':5: Error:' "$work/lineinfo"
$NOCT -j0 -O1 typing/anno_violate.noct \
    > "$work/no-lineinfo" 2>&1 || true
grep -q ':0: Error:' "$work/no-lineinfo"

for option in -O4 -O2foo --optimize-level= \
              --optimize-level=-1 --optimize-level=x \
              --optimize-level=999999999999999999999999; do
    if $NOCT "$option" -j0 simd/f32.noct \
        > "$work/invalid.out" 2>&1; then
        echo "invalid option accepted: $option" >&2
        exit 1
    fi
    grep -q 'Invalid optimize-level option' "$work/invalid.out"
done

for option in --disable-jit --force-jit --jit-threshold=0 \
              --optimize-level=2; do
    if $NOCT "$option" -j0 simd/f32.noct > "$work/removed.out" 2>&1; then
        echo "removed/invalid option accepted: $option" >&2
        exit 1
    fi
done

echo 'All CLI optimization-level tests passed.'
