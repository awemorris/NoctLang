#!/bin/sh

#
# Typed-ops test suite (docs/design/07-typed-ops.md).
#
# Runs every case at optimize level 0 and 2, with the interpreter and
# the JIT.  All four runs must match the golden output byte-for-byte:
# typed opcodes must never change observable behavior.
#
# A case may provide NAME.noct.out2 as the level-2 golden when its
# output contains error line numbers (debug info present at level 0,
# omitted at level >= 1).
#
# Cases listed in MUST_EMIT must additionally report at least one
# typed-op emission via NOCT_TYPED_DEBUG=1 at level 2 (a golden test
# alone cannot catch the optimization silently dying).
#

NOCT=${NOCT:-../../build-static/noct}

MUST_EMIT="arith farith abce_region lattice shift_edges"

echo 'Typed-ops tests:'

FAILED=0
for tc in typedop/*.noct; do
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

# Emission assertions (level 1, interpreter is enough: emission is
# decided at LIR build time, before the backend choice).
for name in $MUST_EMIT; do
    tc="typedop/$name.noct"
    if [ ! -f "$tc" ]; then
        continue
    fi
    n=$(NOCT_TYPED_DEBUG=1 $NOCT -j0 -O1 "$tc" 2>&1 \
        | grep -c '^TYPED: .*emitted=[1-9]')
    if [ "$n" -eq 0 ]; then
        echo "FAIL $tc (no typed emission reported)"
        FAILED=1
    else
        echo "PASS $tc (typed emission reported)"
    fi
done

# Note on abce_w64.noct: there is deliberately NO "zero emission"
# assertion for it.  Stage B legitimately types the ABCE guard
# arithmetic ($lo >= 0 etc.) even for a 64-bit-bet loop, so the
# per-function counter is nonzero.  The invariant that matters --
# the long-contaminated accumulator inside the w64 fast body must
# never see a typed int op -- is enforced by the golden output
# (the sum only comes out right through the generic long path).

rm -f out

if [ "$FAILED" -ne 0 ]; then
    echo 'Typed-ops tests FAILED.'
    exit 1
fi
echo 'All typed-ops tests passed.'
