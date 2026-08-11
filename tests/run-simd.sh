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

MUST_VECTORIZE="blend blend2 blend2_multigroup fma_forms mixed_convert remainder tempafter inplace vshift_edges mixed_bases overlap_dynamic restricted_params f32"
MUST_NOT="overlap_reject u8_reject counter_value carried if_reject budget"

echo 'SIMD tests:'

FAILED=0
for tc in simd/*.noct; do
    for lvl in "-O0" "-O2"; do
        golden="$tc.out"
        if [ "$lvl" = "-O2" ] && [ -f "$tc.out2" ]; then
            golden="$tc.out2"
        fi
        for jit in "-j0" "-j"; do
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

# The x86 tiers deliberately use the SSE2 instruction subset for the SSE3
# ceiling; SSE4.1 only shortens operations such as i32 multiply/extract.
# On non-x86 backends these ceilings safely reduce to the scalar tier.
for tier in scalar sse2 sse3 sse41 avx; do
    NOCT_JIT_SIMD_MAX=$tier $NOCT -j -O2 \
        simd/blend2.noct > out 2>&1
    if ! diff -q simd/blend2.noct.out out > /dev/null 2>&1; then
        echo "FAIL SIMD ceiling $tier"
        diff simd/blend2.noct.out out | head -5
        FAILED=1
    else
        echo "PASS SIMD ceiling $tier"
    fi
done

# The x86_64 LIR plan must retain all four blend common values, and the
# native JIT build must finish rather than silently falling back after a
# code-generation error.
if [ "$(uname -m 2>/dev/null)" = "x86_64" ]; then
    debug=$(NOCT_LIR_VFOR_DEBUG=1 NOCT_JIT_CODEGEN_DEBUG=1 \
        $NOCT -j -O3 simd/blend2.noct 2>&1 >/dev/null)
    if printf '%s\n' "$debug" | grep -q \
        'noct-lir-vfor: max=13 .*caches=4 ' && \
       printf '%s\n' "$debug" | grep -q \
        'noct-jit-codegen: x86_64: func=blend '; then
        echo "PASS x86_64 four-value vector cache/native JIT build"
    else
        echo "FAIL x86_64 four-value vector cache/native JIT build"
        printf '%s\n' "$debug" | head -10
        FAILED=1
    fi
fi

for name in $MUST_VECTORIZE; do
    tc="simd/$name.noct"
    n=$(NOCT_SIMD_DEBUG=1 $NOCT -j0 -O2 "$tc" 2>&1 \
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
    n=$(NOCT_SIMD_DEBUG=1 $NOCT -j0 -O2 "$tc" 2>&1 \
        | grep -c 'vectorized')
    if [ "$n" -ne 0 ]; then
        echo "FAIL $tc (vectorized unexpectedly)"
        FAILED=1
    else
        echo "PASS $tc (stayed scalar, as required)"
    fi
done

# --simd-info is the stable, success-only diagnostic.  It must include
# the source loop line without exposing developer-only rejection output.
info=$($NOCT --simd-info -j0 -O2 \
    simd/f32.noct 2>&1)
if ! printf '%s\n' "$info" | grep -q \
    '^SIMD: simd/f32.noct:6: vectorized (f32x4)$'; then
    echo "FAIL --simd-info source location"
    FAILED=1
elif printf '%s\n' "$info" | grep -q 'rejected'; then
    echo "FAIL --simd-info exposed rejection diagnostics"
    FAILED=1
else
    echo "PASS --simd-info source location"
fi

if $NOCT --simd-info -j0 simd/f32.noct 2>&1 |
    grep -q '^SIMD:'; then
    echo "FAIL --simd-info reported without vectorization"
    FAILED=1
else
    echo "PASS --simd-info success-only behavior"
fi

# O3 contracts eligible FP32 expressions to the common FMA opcode.  Native,
# interpreter, and capability-ceiling paths must retain identical output.
for jit in "-j0" "-j"; do
    $NOCT $jit -O3 simd/blend2.noct > out 2>&1
    if ! diff -q simd/blend2.noct.out out > /dev/null 2>&1; then
        echo "FAIL O3 FMA blend2 ($jit)"
        FAILED=1
    else
        echo "PASS O3 FMA blend2 ($jit)"
    fi
done
for tier in scalar sse41; do
    NOCT_JIT_SIMD_MAX=$tier $NOCT -j -O3 \
        simd/blend2.noct > out 2>&1
    if ! diff -q simd/blend2.noct.out out > /dev/null 2>&1; then
        echo "FAIL O3 FMA fallback ceiling $tier"
        FAILED=1
    else
        echo "PASS O3 FMA fallback ceiling $tier"
    fi
done

# Optimized bytecode must preserve the ABI/prologue vector metadata.
tmp_dir=$(mktemp -d)
cp simd/f32.noct "$tmp_dir/f32.noct"
compile_info=$($NOCT_META --compile --simd-info -O2 \
    "$tmp_dir/f32.noct" 2>&1)
if ! printf '%s\n' "$compile_info" | grep -q \
       "^SIMD: $tmp_dir/f32.noct:6: vectorized (f32x4)$" ||
   ! grep -a -q '^Vector Ops$' "$tmp_dir/f32.nb"; then
    echo "FAIL SIMD bytecode vector metadata/info"
    FAILED=1
else
    $NOCT_META -j "$tmp_dir/f32.nb" > "$tmp_dir/out" 2>&1
    if ! diff -q simd/f32.noct.out "$tmp_dir/out" > /dev/null 2>&1; then
        echo "FAIL SIMD bytecode round trip"
        diff simd/f32.noct.out "$tmp_dir/out" | head -5
        FAILED=1
    else
        echo "PASS SIMD bytecode vector metadata/round trip"
    fi
fi

# O3 bytecode records that the function requires fused semantics.  O2 must
# not acquire that metadata, and the persisted O3 image must round-trip.
cp simd/blend2.noct "$tmp_dir/blend2.noct"
$NOCT_META --compile -O2 "$tmp_dir/blend2.noct" > /dev/null 2>&1
if grep -a -q '^FMA Ops$' "$tmp_dir/blend2.nb"; then
    echo "FAIL O2 bytecode unexpectedly contains FMA metadata"
    FAILED=1
else
    echo "PASS O2 bytecode has no FMA metadata"
fi
$NOCT_META --compile -O3 "$tmp_dir/blend2.noct" > /dev/null 2>&1
if ! grep -a -q '^FMA Ops$' "$tmp_dir/blend2.nb"; then
    echo "FAIL O3 bytecode missing FMA metadata"
    FAILED=1
else
    NOCT_JIT_SIMD_MAX=sse41 $NOCT_META -j \
        "$tmp_dir/blend2.nb" > "$tmp_dir/blend2.out" 2>&1
    if ! diff -q simd/blend2.noct.out "$tmp_dir/blend2.out" > /dev/null 2>&1; then
        echo "FAIL O3 FMA bytecode portable round trip"
        FAILED=1
    else
        echo "PASS O3 FMA bytecode metadata/portable round trip"
    fi
fi
rm -rf "$tmp_dir"

rm -f out

if [ "$FAILED" -ne 0 ]; then
    echo 'SIMD tests FAILED.'
    exit 1
fi
echo 'All SIMD tests passed.'
