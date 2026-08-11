#!/bin/sh

set -eu

NOCT=${NOCT:-../build-mt-debug/noct}
work=".tmp-cli-options-$$"
mkdir "$work"
trap 'rm -rf "$work"' EXIT HUP INT TERM

echo 'CLI optimization-level tests:'

for option in -O0 -O1 -O2 -O3 --optimize-level=4; do
    $NOCT "$option" --disable-jit simd/f32.noct > "$work/out"
    diff simd/f32.noct.out "$work/out"
done

# Compile-mode option order is observable through O3-only FMA metadata.
cp simd/blend2.noct "$work/blend2.noct"
$NOCT --compile -O3 -O2 "$work/blend2.noct"
if grep -a -q '^FMA Ops$' "$work/blend2.nb"; then
    echo 'last-option-wins failed for -O3 -O2' >&2
    exit 1
fi
$NOCT --compile -O2 -O3 "$work/blend2.noct"
grep -a -q '^FMA Ops$' "$work/blend2.nb"

$NOCT --compile --app -O3 "$work/app.nap" simd/f32.noct
test -s "$work/app.nap"
$NOCT --ansic -O3 "$work/f32.c" simd/f32.noct
test -s "$work/f32.c"

for option in -O -O4 -O2foo --optimize-level= \
              --optimize-level=-1 --optimize-level=x \
              --optimize-level=999999999999999999999999; do
    if $NOCT "$option" --disable-jit simd/f32.noct \
        > "$work/invalid.out" 2>&1; then
        echo "invalid option accepted: $option" >&2
        exit 1
    fi
    grep -q 'Invalid optimize-level option' "$work/invalid.out"
done

echo 'All CLI optimization-level tests passed.'
