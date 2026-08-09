#!/bin/sh

#
# SIMD auto-vectorization test suite (docs/design/06-simd.md).
#
# Runs every case at optimize level 0 and 2, with the interpreter and
# the JIT.  All four runs must match the golden output byte-for-byte:
# vectorization must never change observable behavior.
#
# Cases in MUST_VECTORIZE must report a "vectorized" line under
# NOCT_SIMD_DEBUG at level 2 (a golden test alone cannot catch the
# pass silently dying); cases in MUST_NOT must not.
#

NOCT=${NOCT:-../build-static/noct}
NOCT_META=${NOCT_META:-$NOCT}

MUST_VECTORIZE="blend remainder tempafter inplace vshift_edges mixed_bases overlap_dynamic restricted_params f32"
MUST_NOT="overlap_reject u8_reject counter_value carried if_reject budget"

echo 'SIMD tests:'

FAILED=0
for tc in simd/*.noct; do
    for lvl in "" "--optimize-level=2"; do
        golden="$tc.out"
        if [ -n "$lvl" ] && [ -f "$tc.out2" ]; then
            golden="$tc.out2"
        fi
        for jit in "--disable-jit" "--force-jit"; do
            $NOCT $jit $lvl "$tc" > out 2>&1
            if ! diff -q "$golden" out > /dev/null 2>&1; then
                echo "FAIL $tc ($jit $lvl)"
                diff "$golden" out | head -5
                FAILED=1
            fi
        done
    done
    echo "PASS $tc"
done

for name in $MUST_VECTORIZE; do
    tc="simd/$name.noct"
    n=$(NOCT_SIMD_DEBUG=1 $NOCT --disable-jit --optimize-level=2 "$tc" 2>&1 \
        | grep -c 'vectorized')
    if [ "$n" -eq 0 ]; then
        echo "FAIL $tc (did not vectorize)"
        FAILED=1
    else
        echo "PASS $tc (vectorized)"
    fi
done

for name in $MUST_NOT; do
    tc="simd/$name.noct"
    n=$(NOCT_SIMD_DEBUG=1 $NOCT --disable-jit --optimize-level=2 "$tc" 2>&1 \
        | grep -c 'vectorized')
    if [ "$n" -ne 0 ]; then
        echo "FAIL $tc (vectorized unexpectedly)"
        FAILED=1
    else
        echo "PASS $tc (stayed scalar, as required)"
    fi
done

# Optimized bytecode must preserve the ABI/prologue vector metadata.
tmp_dir=$(mktemp -d)
cp simd/f32.noct "$tmp_dir/f32.noct"
if ! $NOCT_META --compile --optimize-level=2 "$tmp_dir/f32.noct" ||
   ! grep -a -q '^Vector Ops$' "$tmp_dir/f32.nb"; then
    echo "FAIL SIMD bytecode vector metadata"
    FAILED=1
else
    $NOCT_META --force-jit "$tmp_dir/f32.nb" > "$tmp_dir/out" 2>&1
    if ! diff -q simd/f32.noct.out "$tmp_dir/out" > /dev/null 2>&1; then
        echo "FAIL SIMD bytecode round trip"
        diff simd/f32.noct.out "$tmp_dir/out" | head -5
        FAILED=1
    else
        echo "PASS SIMD bytecode vector metadata/round trip"
    fi
fi
rm -rf "$tmp_dir"

rm -f out

if [ "$FAILED" -ne 0 ]; then
    echo 'SIMD tests FAILED.'
    exit 1
fi
echo 'All SIMD tests passed.'
