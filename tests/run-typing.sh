#!/bin/sh

#
# Type-annotation test suite (docs/design/02-typing.md).
# Annotations are inert below level 2; at level 2 annotated params get
# entry checks.  A case may provide NAME.noct.out2 as the level-2
# golden (used by the intentional violation case).
#

NOCT=${NOCT:-../build-static/noct}

echo 'Typing tests:'

FAILED=0
for tc in typing/*.noct; do
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
    rm -f out
    echo "PASS $tc"
done

if [ "$FAILED" -ne 0 ]; then
    echo 'Typing tests failed.'
    exit 1
fi
echo 'All typing tests passed.'
